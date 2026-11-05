#include "Craig_Audio.hpp"
#include "Craig_Logger.hpp"
#include "Craig_AcousticMaterial.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>

//The callback can't ever wait on a lock, so these have to be real lock free atomics
static_assert(std::atomic<uint8_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);

CraigError Craig::Audio::init() {
	if (m_initialised) return CRAIG_SUCCESS;

	//The window only starts SDL video, audio's its own subsystem
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		Logger::audio().warn("Audio subsystem unavailable: {}", SDL_GetError());
		return CRAIG_FAIL;
	}
	m_initialised = true;
	return CRAIG_SUCCESS;
}

CraigError Craig::Audio::loadWave(const std::string& path) {
	SDL_AudioSpec source{};
	Uint8* bytes = nullptr;
	Uint32 length = 0;
	if (!SDL_LoadWAV(path.c_str(), &source, &bytes, &length)) {
		Logger::audio().warn("Couldn't load {}: {}", path, SDL_GetError());
		return CRAIG_FAIL;
	}

	//Everything gets turned into mono floats at 48 kHz, the mixer resamples from there
	const SDL_AudioSpec mono{SDL_AUDIO_F32, 1, kWaveRate};
	Uint8* converted = nullptr;
	int convertedLength = 0;
	const bool validLength = length > 0 && length <= static_cast<Uint32>(std::numeric_limits<int>::max());
	const bool convertedOK = validLength && SDL_ConvertAudioSamples(&source, bytes, static_cast<int>(length), &mono, &converted, &convertedLength);
	SDL_free(bytes);
	if (!convertedOK || convertedLength <= 0 || convertedLength % sizeof(float) != 0) {
		Logger::audio().warn("Couldn't convert {} to mono float audio: {}", path, SDL_GetError());
		SDL_free(converted);
		return CRAIG_FAIL;
	}

	mv_wave.resize(static_cast<size_t>(convertedLength) / sizeof(float));
	std::memcpy(mv_wave.data(), converted, convertedLength);
	SDL_free(converted);

	if (!std::all_of(mv_wave.begin(), mv_wave.end(), [](float sample) { return std::isfinite(sample) && std::abs(sample) <= 1.0f; })) {
		Logger::audio().warn("{} contains invalid/out-of-range samples", path);
		mv_wave.clear();
		return CRAIG_FAIL;
	}
	Logger::audio().info("Loaded {}: {} Hz, {} channel(s), converted to {} mono frames", path, source.freq, source.channels, mv_wave.size());
	return CRAIG_SUCCESS;
}

CraigError Craig::Audio::playLoop(const std::string& path) { return openSound(path, true); }

CraigError Craig::Audio::prepareOneShot(const std::string& path) { return openSound(path, false); }

void Craig::Audio::playOneShot() {
	if (!m_playing || m_looping) return;
	const uint64_t command = m_playbackCommand.load(std::memory_order_relaxed);
	m_playbackCommand.store((command & ~kShotCountMask) | static_cast<uint32_t>(command + 1), std::memory_order_release);
}

void Craig::Audio::cancelOneShot() {
	const uint64_t command = m_playbackCommand.load(std::memory_order_relaxed);
	m_playbackCommand.store((command & ~kShotCountMask) + (kShotCountMask + 1), std::memory_order_release);
}

CraigError Craig::Audio::openSound(const std::string& path, bool looping) {
	//Only ever called from the main thread, the old callback gets taken off before we touch any of its memory
	terminate();
	if (init() != CRAIG_SUCCESS) return CRAIG_FAIL;
	if (loadWave(path) != CRAIG_SUCCESS) {
		terminate();
		return CRAIG_FAIL;
	}

	//Allocate everything the callback needs BEFORE the callback exists
	for (auto& line : mv_delayLines) line.assign(kDelayCapacity, 0.0f);
	m_writeIndex = 0;
	m_looping = looping;
	//A one shot starts finished, it waits for a press
	m_cursor = looping ? 0.0 : static_cast<double>(mv_wave.size());
	m_playbackCommand.store(0, std::memory_order_relaxed);
	m_consumedPlaybackCommand = 0;
	m_voices = {};
	m_delayFrames = kDelayCapacity;
	m_lowSplit = 0.0f;
	m_highSplit = 0.0f;
	m_gain = 0.0f;
	m_fade = 1.0f;
	m_current = {};
	m_previous = {};
	m_mailbox.reset();
	m_reportedGain.store(0.0f, std::memory_order_relaxed);
	m_reportedArrivals.store(0, std::memory_order_relaxed);
	m_reverb.start();
	m_reverbActive = false;
	m_nextReverbUpdate = 0;

	const SDL_AudioSpec requested{SDL_AUDIO_F32, 2, kWaveRate};
	m_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested);
	if (!m_device || !SDL_SetAudioPostmixCallback(m_device, postmix, this) || !SDL_ResumeAudioDevice(m_device)) {
		Logger::audio().warn("Playback unavailable: {}", SDL_GetError());
		terminate();
		return CRAIG_FAIL;
	}
	m_playing = true;
	m_state = AudioStatus::State::NoResult;
	Logger::audio().info("{} ready on SDL {} (silent until there's a fresh result)", looping ? "Loop" : "One-shot sound", SDL_GetCurrentAudioDriver());
	return CRAIG_SUCCESS;
}

void Craig::Audio::publish(const SoundProbeReadout* probe, uint64_t deadlineMs) {
	ArrivalSet& set = m_mailbox.back();
	set.audible = probe != nullptr;
	set.deadlineMs = deadlineMs;
	set.count = 0;
	if (probe) {
		for (uint32_t i = 0; i < probe->arrivalCount; ++i) {
			const SoundArrival& arrival = probe->arrivals[i];
			if (arrival.order == 0 ? !m_directEnabled : !m_reflectionsEnabled) continue;
			//Equal power pan, no HRTF or delay between the ears yet
			const float angle = (arrival.pan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
			set.taps[set.count++] = {arrival.delaySeconds, arrival.gains, std::cos(angle), std::sin(angle)};
		}
	}
	m_mailbox.publish();
}

void Craig::Audio::invalidate() {
	//Presses from before a pause/scene change shouldn't go off later on
	cancelOneShot();
	m_reverb.invalidate();
	m_reverbActive = false;
	m_nextReverbUpdate = 0;
	publish(nullptr, 0);
	m_state = AudioStatus::State::NoResult;
}

void Craig::Audio::stop() {
	invalidate();
	m_playing = false;
	m_state = AudioStatus::State::Stopped;
}

void Craig::Audio::setMuted(bool muted) {
	m_muted = muted;
	if (muted) {
		invalidate();
		m_state = AudioStatus::State::Muted;
	}
}

void Craig::Audio::setPathsEnabled(bool direct, bool reflections) {
	m_directEnabled = direct;
	m_reflectionsEnabled = reflections;
}

void Craig::Audio::update(const SoundProbeReadout& probe, bool worldRunning) {
	//Don't trust anything weird coming out of the readout
	bool valid = probe.hasResult && std::isfinite(probe.ageMs) && probe.ageMs >= 0.0 && probe.arrivalCount <= kMaxSoundArrivals;
	for (uint32_t i = 0; valid && i < probe.arrivalCount; ++i) {
		const SoundArrival& arrival = probe.arrivals[i];
		valid = std::isfinite(arrival.delaySeconds) && arrival.delaySeconds >= 0.0f && std::isfinite(arrival.pan);
		for (float gain : arrival.gains) valid = valid && std::isfinite(gain) && gain >= 0.0f;
	}

	if (!m_playing || !m_device) m_state = AudioStatus::State::Stopped;
	else if (!worldRunning) m_state = AudioStatus::State::Paused;
	else if (m_muted) m_state = AudioStatus::State::Muted;
	else if (probe.state != SoundProbeReadout::State::Ready && probe.state != SoundProbeReadout::State::Waiting) m_state = AudioStatus::State::NoResult;
	else if (!valid) m_state = AudioStatus::State::NoResult;
	else if (probe.ageMs >= kMaxResultAgeMs) m_state = AudioStatus::State::Stale;
	else {
		m_state = probe.obstruction == 0.0f ? AudioStatus::State::Clear :
			(probe.obstruction == 1.0f ? AudioStatus::State::Blocked : AudioStatus::State::Partial);
		//Rebuilding the response is slow, so at most 4 times a second
		const bool tail = m_reflectionsEnabled && probe.reverbEnabled;
		if (tail && SDL_GetTicks() >= m_nextReverbUpdate) {
			m_reverb.submit(probe.reverbEnergy, probe.arrivalCount ? probe.arrivals[0].delaySeconds : 0.0f);
			m_nextReverbUpdate = SDL_GetTicks() + 250;
		}
		if (!tail && m_reverbActive) m_reverb.invalidate();
		m_reverbActive = tail;
		//The deadline comes from when the result was made, publishing the same old result again doesn't make it last longer
		publish(&probe, SDL_GetTicks() + static_cast<uint64_t>(kMaxResultAgeMs - probe.ageMs));
		return;
	}
	cancelOneShot();
	if (m_reverbActive) m_reverb.invalidate();
	m_reverbActive = false;
	m_nextReverbUpdate = 0;
	publish(nullptr, 0);
}

Craig::AudioStatus Craig::Audio::getStatus() const {
	AudioStatus status;
	status.available = m_device != 0;
	status.state = status.available ? m_state : AudioStatus::State::Unavailable;
	status.muted = m_muted;
	status.directEnabled = m_directEnabled;
	status.reflectionsEnabled = m_reflectionsEnabled;
	status.reverbReady = m_reverb.ready();
	status.reverbFailed = m_reverb.failed();
	status.gain = m_reportedGain.load(std::memory_order_relaxed);
	status.arrivals = m_reportedArrivals.load(std::memory_order_relaxed);
	return status;
}

const char* Craig::Audio::stateName(AudioStatus::State state) {
	const char* names[] = {"unavailable", "stopped", "paused", "waiting for a valid result", "stale (fading to silence)", "muted",
		"direct path clear", "direct path blocked", "direct path partly blocked"};
	return names[static_cast<unsigned>(state)];
}

void SDLCALL Craig::Audio::postmix(void* userdata, const SDL_AudioSpec* spec, float* buffer, int bytes) {
	if (spec->freq <= 0 || spec->channels <= 0 || bytes <= 0) return;
	static_cast<Audio*>(userdata)->mix(*spec, buffer, bytes, SDL_GetTicks());
}

void Craig::Audio::renderTaps(const RenderSet& set, float weight, uint32_t writeIndex, float& left, float& right) const {
	if (weight <= 0.0f) return;
	constexpr uint32_t mask = kDelayCapacity - 1;
	for (uint32_t i = 0; i < set.count; ++i) {
		//A cancelled shot is still sitting in the delay line, don't read further back than the cancel
		if (set.delays[i] > m_delayFrames) continue;
		const uint32_t read = (writeIndex - set.delays[i]) & mask;
		const Tap& tap = set.taps[i];
		const float sample = weight * (tap.gains[0] * mv_delayLines[0][read] + tap.gains[1] * mv_delayLines[1][read] +
			tap.gains[2] * mv_delayLines[2][read]);
		left += sample * tap.left;
		right += sample * tap.right;
	}
}

void Craig::Audio::mix(const SDL_AudioSpec& spec, float* buffer, int bytes, uint64_t nowMs) {
	if (spec.freq <= 0 || spec.channels <= 0 || bytes <= 0 || mv_wave.empty()) return;
	//Postmix lets us write straight into SDL's buffer, there's no stream bound to this device so it starts silent
	//No allocating, logging, locks or Vulkan in here - https://wiki.libsdl.org/SDL3/SDL_SetAudioPostmixCallback
	const int frames = bytes / static_cast<int>(sizeof(float)) / spec.channels;
	const double step = static_cast<double>(kWaveRate) / spec.freq;
	const auto readSample = [&](double& cursor, bool looping) {
		if (cursor >= mv_wave.size()) return 0.0f;
		const size_t index = static_cast<size_t>(cursor);
		const float fraction = static_cast<float>(cursor - index);
		const float next = index + 1 < mv_wave.size() ? mv_wave[index + 1] : (looping ? mv_wave[0] : 0.0f);
		const float sample = std::lerp(mv_wave[index], next, fraction);
		cursor += step;
		if (looping && cursor >= mv_wave.size()) cursor = std::fmod(cursor, static_cast<double>(mv_wave.size()));
		return sample;
	};
	//One pole low passes at 800 Hz and 8 kHz, subtracting them gives the three bands
	const auto splitCoefficient = [&spec](float cutoff) {
		return 1.0f - std::exp(-2.0f * std::numbers::pi_v<float> * std::min(cutoff, 0.45f * spec.freq) / spec.freq);
	};
	const float lowCoefficient = splitCoefficient(kAcousticBandSplits[0]);
	const float highCoefficient = splitCoefficient(kAcousticBandSplits[1]);
	const float fadeStep = 1.0f / (kTransitionSeconds * spec.freq);
	const float riseStep = 1.0f / (kTransitionSeconds * spec.freq);
	const float silenceStep = 1.0f / (kSilenceFadeSeconds * spec.freq);

	for (int i = 0; i < frames; ++i) {
		//Swapping sets halfway through a crossfade would pop, so a newer one waits until this one's done
		if (m_fade >= 1.0f && m_mailbox.consume()) {
			const ArrivalSet& next = m_mailbox.front();
			m_previous = m_current;
			m_current.audible = next.audible;
			m_current.deadlineMs = next.deadlineMs;
			m_current.count = next.count;
			for (uint32_t tap = 0; tap < next.count; ++tap) {
				m_current.taps[tap] = next.taps[tap];
				const double delay = std::round(static_cast<double>(next.taps[tap].delaySeconds) * spec.freq);
				m_current.delays[tap] = static_cast<uint32_t>(std::min(delay, static_cast<double>(kDelayCapacity - 1)));
			}
			m_fade = 0.0f;
		}
		const double sampleTimeMs = static_cast<double>(nowMs) + i * 1000.0 / spec.freq;
		const bool audible = m_current.audible && sampleTimeMs < m_current.deadlineMs;
		m_gain = audible ? std::min(1.0f, m_gain + riseStep) : std::max(0.0f, m_gain - silenceStep);

		if (!m_looping) {
			const uint64_t command = m_playbackCommand.load(std::memory_order_acquire);
			//New cancel generation, stop every voice and forget what's in the delay lines
			if ((command & ~kShotCountMask) != (m_consumedPlaybackCommand & ~kShotCountMask)) {
				m_voices = {};
				m_lowSplit = m_highSplit = 0.0f;
				m_delayFrames = 0;
				m_consumedPlaybackCommand = command & ~kShotCountMask;
			}
			//Only start once the paths are actually audible, otherwise the start of the sound gets faded in
			uint32_t pending = static_cast<uint32_t>(command) - static_cast<uint32_t>(m_consumedPlaybackCommand);
			if (pending && audible && m_gain >= 0.999f) {
				//If all 16 are busy extra presses get ignored, better than cutting one off halfway
				for (auto& voice : m_voices) {
					if (!pending) break;
					if (!voice.active) { voice = {0.0, true}; --pending; }
				}
				m_consumedPlaybackCommand = command;
			}
		}
		//All the voices come from the same source, so they can just be added together before the propagation
		float dry = m_looping ? readSample(m_cursor, true) : 0.0f;
		if (!m_looping) {
			for (auto& voice : m_voices) {
				if (!voice.active) continue;
				dry += readSample(voice.cursor, false);
				voice.active = voice.cursor < mv_wave.size();
			}
		}
		//Finished voices feed in zeros, so the delays can still play out
		m_lowSplit += lowCoefficient * (dry - m_lowSplit);
		m_highSplit += highCoefficient * (dry - m_highSplit);
		constexpr uint32_t mask = kDelayCapacity - 1;
		mv_delayLines[0][m_writeIndex] = m_lowSplit;
		mv_delayLines[1][m_writeIndex] = m_highSplit - m_lowSplit;
		mv_delayLines[2][m_writeIndex] = dry - m_highSplit;

		float left = 0.0f;
		float right = 0.0f;
		//Upstream's reconstruction already filters and adds up the bands, splitting the input again would colour it twice
		m_reverb.processSample(dry, spec.freq, left, right);
		renderTaps(m_current, m_fade, m_writeIndex, left, right);
		renderTaps(m_previous, 1.0f - m_fade, m_writeIndex, left, right);
		m_fade = std::min(1.0f, m_fade + fadeStep);
		m_writeIndex = (m_writeIndex + 1) & mask;
		m_delayFrames = std::min(kDelayCapacity, m_delayFrames + 1);

		//Clamp instead of letting it wrap round into noise
		left = std::clamp(left * kOutputGain * m_gain, -1.0f, 1.0f);
		right = std::clamp(right * kOutputGain * m_gain, -1.0f, 1.0f);
		if (spec.channels == 1) buffer[i] += 0.5f * (left + right);
		else {
			buffer[i * spec.channels] += left;
			buffer[i * spec.channels + 1] += right;
		}
	}
	m_reportedGain.store(m_gain, std::memory_order_relaxed);
	m_reportedArrivals.store(m_current.count, std::memory_order_relaxed);
}

CraigError Craig::Audio::terminate() {
	stop();
	if (m_device) {
		//SDL waits for a callback that's running before it swaps it out
		SDL_SetAudioPostmixCallback(m_device, nullptr, nullptr);
		SDL_CloseAudioDevice(m_device);
		m_device = 0;
	}
	m_reverb.stop();
	mv_wave.clear();
	for (auto& line : mv_delayLines) line = {};
	if (m_initialised) SDL_QuitSubSystem(SDL_INIT_AUDIO);
	m_initialised = false;
	return CRAIG_SUCCESS;
}
