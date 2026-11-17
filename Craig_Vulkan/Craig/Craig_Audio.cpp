#include "Craig_Audio.hpp"
#include "Craig_Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

//The callback can't ever wait on a lock, so these have to be real lock free atomics
static_assert(std::atomic<uint8_t>::is_always_lock_free);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
static_assert(std::atomic<float>::is_always_lock_free);

namespace {
	IPLVector3 toIPL(const std::array<float, 3>& v) { return {v[0], v[1], v[2]}; }
	const IPLSpeakerLayout kStereo{IPL_SPEAKERLAYOUTTYPE_STEREO, 0, nullptr};
}

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

	//Everything gets turned into mono floats at Steam Audio's rate, so the effects never resample
	const SDL_AudioSpec mono{SDL_AUDIO_F32, 1, kRate};
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

CraigError Craig::Audio::createEffects() {
	const SteamAudio& steamAudio = SteamAudio::getInstance();
	if (!steamAudio.isReady()) {
		Logger::audio().warn("Playback unavailable: Steam Audio didn't start");
		return CRAIG_FAIL;
	}
	IPLContext context = steamAudio.getContext();
	IPLAudioSettings settings = steamAudio.getAudioSettings();

	IPLDirectEffectSettings directSettings{1};
	IPLBinauralEffectSettings binauralSettings{steamAudio.getHRTF()};
	IPLPanningEffectSettings panningSettings{kStereo};
	IPLAmbisonicsDecodeEffectSettings decodeSettings{kStereo, steamAudio.getHRTF(), SteamAudio::kMaxOrder};
	if (iplDirectEffectCreate(context, &settings, &directSettings, &m_directEffect) != IPL_STATUS_SUCCESS ||
		iplBinauralEffectCreate(context, &settings, &binauralSettings, &m_binauralEffect) != IPL_STATUS_SUCCESS ||
		iplPanningEffectCreate(context, &settings, &panningSettings, &m_panningEffect) != IPL_STATUS_SUCCESS ||
		iplAmbisonicsDecodeEffectCreate(context, &settings, &decodeSettings, &m_decodeEffect) != IPL_STATUS_SUCCESS) {
		Logger::audio().warn("Couldn't create the Steam Audio effects");
		releaseEffects();
		return CRAIG_FAIL;
	}
	return CRAIG_SUCCESS;
}

void Craig::Audio::releaseEffects() {
	if (m_directEffect) iplDirectEffectRelease(&m_directEffect);
	if (m_binauralEffect) iplBinauralEffectRelease(&m_binauralEffect);
	if (m_panningEffect) iplPanningEffectRelease(&m_panningEffect);
	if (m_decodeEffect) iplAmbisonicsDecodeEffectRelease(&m_decodeEffect);
	m_directEffect = nullptr;
	m_binauralEffect = nullptr;
	m_panningEffect = nullptr;
	m_decodeEffect = nullptr;
}

CraigError Craig::Audio::playLoop(const std::string& path) { return openSound(path, true); }

CraigError Craig::Audio::prepareOneShot(const std::string& path) { return openSound(path, false); }

void Craig::Audio::playOneShot(float gain) {
	if (!m_playing || m_looping || !std::isfinite(gain)) return;
	const uint64_t command = m_playbackCommand.load(std::memory_order_relaxed);
	const uint64_t strength = static_cast<uint64_t>(std::lround(std::clamp(gain, 0.0f, 1.0f) * 255.0f));
	m_playbackCommand.store((command & kShotGenerationMask) | (strength << 24) | ((command + 1) & kShotCountMask), std::memory_order_release);
}

void Craig::Audio::cancelOneShot() {
	const uint64_t command = m_playbackCommand.load(std::memory_order_relaxed);
	m_playbackCommand.store((command & kShotGenerationMask) + (1ull << 32), std::memory_order_release);
}

CraigError Craig::Audio::openSound(const std::string& path, bool looping) {
	//Only ever called from the main thread, the old stream gets destroyed before we touch any of its memory
	terminate();
	if (init() != CRAIG_SUCCESS) return CRAIG_FAIL;
	if (loadWave(path) != CRAIG_SUCCESS || createEffects() != CRAIG_SUCCESS) {
		terminate();
		return CRAIG_FAIL;
	}

	//Allocate everything the callback needs BEFORE the callback exists
	m_looping = looping;
	//A one shot starts finished, it waits for a press
	m_cursor = looping ? 0.0 : static_cast<double>(mv_wave.size());
	m_playbackCommand.store(0, std::memory_order_relaxed);
	m_consumedPlaybackCommand = 0;
	m_voices = {};
	m_gain = 0.0f;
	m_resetEffects = false;
	m_current = {};
	m_mailbox.reset();
	m_callbackFrames.store(0, std::memory_order_relaxed);
	m_callbackBufferFrames.store(0, std::memory_order_relaxed);
	m_callbackNs.store(0, std::memory_order_relaxed);
	m_callbackPeakLoad.store(0.0f, std::memory_order_relaxed);
	m_callbackOverBudget.store(0, std::memory_order_relaxed);
	m_callbackMaxGapNs.store(0, std::memory_order_relaxed);
	m_callbackLongGaps.store(0, std::memory_order_relaxed);
	m_previousCallbackStartNs = m_previousCallbackBudgetNs = 0;
	m_nextTimingLogMs = m_lastLoggedOverBudget = 0;
	m_reportedGain.store(0.0f, std::memory_order_relaxed);
	m_reportedOrder.store(0, std::memory_order_relaxed);
	m_reverb.start();
	m_reverbActive = false;
	m_reverbOrder = 0;
	m_nextReverbUpdate = 0;

	//A stream instead of the old postmix callback, SDL resamples it to whatever the device runs at
	//Steam Audio's effects are made for one rate and frame size, the postmix buffer could be anything
	const SDL_AudioSpec requested{SDL_AUDIO_F32, 2, kRate};
	mp_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested, streamCallback, this);
	if (!mp_stream || !SDL_ResumeAudioStreamDevice(mp_stream)) {
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
	ParameterSet& set = m_mailbox.back();
	set.audible = probe != nullptr;
	set.deadlineMs = deadlineMs;
	set.direct = m_directEnabled;
	set.reflections = m_reflectionsEnabled && m_reverbActive;
	set.binaural = m_binaural;
	set.reflectionOrder = m_reverbOrder;
	if (probe) {
		const DirectPath& direct = probe->direct;
		IPLDirectEffectParams& params = set.directParams;
		params = {};
		params.flags = static_cast<IPLDirectEffectFlags>(IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION | IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION |
			IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION | IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);
		params.transmissionType = IPL_TRANSMISSIONTYPE_FREQDEPENDENT;
		params.distanceAttenuation = direct.distanceAttenuation;
		params.directivity = 1.0f;
		params.occlusion = direct.occlusion;
		for (int band = 0; band < 3; ++band) {
			params.airAbsorption[band] = direct.airAbsorption[band];
			params.transmission[band] = direct.transmission[band];
		}
		const ListenerFrame& listener = probe->listener;
		set.listener = {toIPL(listener.right), toIPL(listener.up), toIPL(listener.ahead), toIPL(listener.origin)};
		//The HRTF wants it relative to where the listener's facing
		set.direction = iplCalculateRelativeDirection(SteamAudio::getInstance().getContext(), toIPL(probe->source),
			toIPL(listener.origin), toIPL(listener.ahead), toIPL(listener.up));
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
	const DirectPath& direct = probe.direct;
	bool valid = probe.hasResult && std::isfinite(probe.ageMs) && probe.ageMs >= 0.0 &&
		std::isfinite(direct.distanceAttenuation) && std::isfinite(direct.occlusion);
	for (int band = 0; valid && band < 3; ++band) {
		valid = std::isfinite(direct.airAbsorption[band]) && std::isfinite(direct.transmission[band]);
	}
	for (const auto* vector : {&probe.source, &probe.listener.origin, &probe.listener.ahead, &probe.listener.up}) {
		for (float value : *vector) valid = valid && std::isfinite(value);
	}

	if (!m_playing || !mp_stream) m_state = AudioStatus::State::Stopped;
	else if (!worldRunning) m_state = AudioStatus::State::Paused;
	else if (m_muted) m_state = AudioStatus::State::Muted;
	else if (probe.state != SoundProbeReadout::State::Ready && probe.state != SoundProbeReadout::State::Waiting) m_state = AudioStatus::State::NoResult;
	else if (!valid) m_state = AudioStatus::State::NoResult;
	else if (probe.ageMs >= kMaxResultAgeMs) m_state = AudioStatus::State::Stale;
	else {
		m_state = probe.obstruction == 0.0f ? AudioStatus::State::Clear :
			(probe.obstruction == 1.0f ? AudioStatus::State::Blocked : AudioStatus::State::Partial);
		const bool tail = m_reflectionsEnabled && probe.reverbEnabled && probe.reflections;
		//Banks of different orders can't crossfade into one decoder, so a new order starts from silence
		if (tail && probe.reflections->order != m_reverbOrder) {
			m_reverb.invalidate();
			m_reverbOrder = probe.reflections->order;
			m_nextReverbUpdate = 0;
		}
		//Rebuilding the response is slow, so at most 4 times a second
		if (tail && SDL_GetTicks() >= m_nextReverbUpdate) {
			m_reverb.submit(probe.reflections);
			m_nextReverbUpdate = SDL_GetTicks() + 250;
		}
		if (!tail && m_reverbActive) m_reverb.invalidate();
		m_reverbActive = tail;
		//The deadline comes from when the result was made, publishing the same old result again doesn't make it last longer
		publish(&probe, SDL_GetTicks() + static_cast<uint64_t>(kMaxResultAgeMs - probe.ageMs));
		logDiagnostics();
		return;
	}
	cancelOneShot();
	if (m_reverbActive) m_reverb.invalidate();
	m_reverbActive = false;
	m_nextReverbUpdate = 0;
	publish(nullptr, 0);
	logDiagnostics();
}

void Craig::Audio::logDiagnostics() {
	if (!m_playing || !mp_stream) return;
	const uint64_t now = SDL_GetTicks();
	if (now < m_nextTimingLogMs) return;
	m_nextTimingLogMs = now + 1000;
	const auto status = getStatus();
	Logger::audio().info("Audio timing: tail {} (order {}), {}, {} Hz / {} frames, callback {:.3f}/{:.3f} ms, peak load {:.1f}%, "
		"over budget {} (+{}), long gaps {} (max {:.3f} ms), tail banks {}, changes {}, invalidations {}",
		m_reverbActive ? "on" : "off", status.reflectionOrder, stateName(status.state), status.callbackRate, status.callbackBufferFrames,
		status.callbackMs, status.callbackBudgetMs, 100.0f * status.callbackPeakLoad, status.callbackOverBudget,
		status.callbackOverBudget - m_lastLoggedOverBudget, status.callbackLongGaps, status.callbackMaxGapMs,
		status.reverbBanks, status.reverbChanges, status.reverbInvalidations);
	m_lastLoggedOverBudget = status.callbackOverBudget;
}

Craig::AudioStatus Craig::Audio::getStatus() const {
	AudioStatus status;
	status.available = mp_stream != nullptr;
	status.state = status.available ? m_state : AudioStatus::State::Unavailable;
	status.muted = m_muted;
	status.directEnabled = m_directEnabled;
	status.reflectionsEnabled = m_reflectionsEnabled;
	status.binaural = m_binaural;
	status.reverbReady = m_reverb.ready();
	status.reverbFailed = m_reverb.failed();
	status.gain = m_reportedGain.load(std::memory_order_relaxed);
	status.reflectionOrder = m_reportedOrder.load(std::memory_order_relaxed);
	status.callbackFrames = m_callbackFrames.load(std::memory_order_relaxed);
	status.callbackRate = kRate;
	status.callbackBufferFrames = m_callbackBufferFrames.load(std::memory_order_relaxed);
	status.callbackMs = static_cast<float>(m_callbackNs.load(std::memory_order_relaxed)) / 1000000.0f;
	status.callbackBudgetMs = 1000.0f * status.callbackBufferFrames / kRate;
	status.callbackPeakLoad = m_callbackPeakLoad.load(std::memory_order_relaxed);
	status.callbackOverBudget = m_callbackOverBudget.load(std::memory_order_relaxed);
	status.callbackMaxGapMs = static_cast<float>(m_callbackMaxGapNs.load(std::memory_order_relaxed)) / 1000000.0f;
	status.callbackLongGaps = m_callbackLongGaps.load(std::memory_order_relaxed);
	status.reverbBanks = m_reverb.activeBanks();
	status.reverbChanges = m_reverb.changes();
	status.reverbInvalidations = m_reverb.invalidations();
	return status;
}

const char* Craig::Audio::stateName(AudioStatus::State state) {
	const char* names[] = {"unavailable", "stopped", "paused", "waiting for a valid result", "stale (fading to silence)", "muted",
		"direct path clear", "direct path blocked", "direct path partly blocked"};
	return names[static_cast<unsigned>(state)];
}

void SDLCALL Craig::Audio::streamCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount) {
	(void)totalAmount;
	if (additionalAmount <= 0) return;
	auto& audio = *static_cast<Audio*>(userdata);
	const uint64_t start = SDL_GetTicksNS();
	//Whole frames only, the stream keeps whatever's left over for next time
	constexpr int blockBytes = static_cast<int>(kFrame * 2 * sizeof(float));
	const int blocks = (additionalAmount + blockBytes - 1) / blockBytes;
	for (int block = 0; block < blocks; ++block) {
		audio.renderBlock(SDL_GetTicks());
		SDL_PutAudioStreamData(stream, audio.m_interleaved.data(), blockBytes);
	}
	audio.recordCallback(start, SDL_GetTicksNS(), static_cast<uint32_t>(blocks) * kFrame);
}

//How long the callback took against how long its audio lasts, if it goes over we're probably crackling
void Craig::Audio::recordCallback(uint64_t startNs, uint64_t endNs, uint32_t frames) {
	const uint64_t duration = endNs - startNs;
	const uint64_t budget = 1000000000ull * frames / kRate;
	const float load = static_cast<float>(duration) / budget;
	m_callbackBufferFrames.store(frames, std::memory_order_relaxed);
	m_callbackNs.store(duration, std::memory_order_relaxed);
	if (load > m_callbackPeakLoad.load(std::memory_order_relaxed)) m_callbackPeakLoad.store(load, std::memory_order_relaxed);
	if (duration > budget) m_callbackOverBudget.fetch_add(1, std::memory_order_relaxed);
	if (m_previousCallbackStartNs) {
		const uint64_t gap = startNs - m_previousCallbackStartNs;
		if (gap > m_callbackMaxGapNs.load(std::memory_order_relaxed)) m_callbackMaxGapNs.store(gap, std::memory_order_relaxed);
		//SDL can batch callbacks up, so a long gap is just a clue, not proof it dropped out
		if (gap > 2 * m_previousCallbackBudgetNs) m_callbackLongGaps.fetch_add(1, std::memory_order_relaxed);
	}
	m_previousCallbackStartNs = startNs;
	m_previousCallbackBudgetNs = budget;
	m_callbackFrames.fetch_add(frames, std::memory_order_relaxed);
}

//The mono sound before any propagation, plus the fade in/out per sample
void Craig::Audio::renderDry(bool audible) {
	const auto readSample = [&](double& cursor, bool looping) {
		if (cursor >= mv_wave.size()) return 0.0f;
		const size_t index = static_cast<size_t>(cursor);
		const float sample = mv_wave[index];
		cursor += 1.0;
		if (looping && cursor >= mv_wave.size()) cursor = 0.0;
		return sample;
	};
	const float riseStep = 1.0f / (kRiseSeconds * kRate);
	const float silenceStep = 1.0f / (kSilenceFadeSeconds * kRate);

	for (uint32_t i = 0; i < kFrame; ++i) {
		m_gain = audible ? std::min(1.0f, m_gain + riseStep) : std::max(0.0f, m_gain - silenceStep);
		m_gainRamp[i] = m_gain;

		if (!m_looping) {
			const uint64_t command = m_playbackCommand.load(std::memory_order_acquire);
			//New cancel generation, stop every voice and forget what's still ringing in the effects
			if ((command & kShotGenerationMask) != (m_consumedPlaybackCommand & kShotGenerationMask)) {
				m_voices = {};
				m_resetEffects = true;
				m_consumedPlaybackCommand = command & kShotGenerationMask;
			}
			//Only start once the paths are actually audible, otherwise the start of the sound gets faded in
			uint32_t pending = static_cast<uint32_t>((command - m_consumedPlaybackCommand) & kShotCountMask);
			// presses queued together share the latest strength, existing voices keep their own
			const float strength = static_cast<float>((command >> 24) & 0xff) / 255.0f;
			if (pending && audible && m_gain >= 0.999f) {
				//If all 16 are busy extra presses get ignored, better than cutting one off halfway
				for (auto& voice : m_voices) {
					if (!pending) break;
					if (!voice.active) { voice = {0.0, true, strength}; --pending; }
				}
				m_consumedPlaybackCommand = command;
			}
		}
		//All the voices come from the same source, so they can just be added together before the propagation
		float dry = m_looping ? readSample(m_cursor, true) : 0.0f;
		if (!m_looping) {
			for (auto& voice : m_voices) {
				if (!voice.active) continue;
				dry += voice.gain * readSample(voice.cursor, false);
				voice.active = voice.cursor < mv_wave.size();
			}
		}
		m_dry[i] = dry;
	}
}

void Craig::Audio::renderBlock(uint64_t nowMs) {
	//No allocating, logging, locks or Vulkan in here, Steam Audio's effects are made for the audio thread
	if (m_mailbox.consume()) m_current = m_mailbox.front();
	const bool audible = m_current.audible && nowMs < m_current.deadlineMs;
	renderDry(audible);

	if (m_resetEffects) {
		iplDirectEffectReset(m_directEffect);
		iplBinauralEffectReset(m_binauralEffect);
		iplPanningEffectReset(m_panningEffect);
		iplAmbisonicsDecodeEffectReset(m_decodeEffect);
		m_resetEffects = false;
	}

	float* dryPointer = m_dry.data();
	float* directPointer = m_directMono.data();
	float* directStereo[2] = {m_directStereo[0].data(), m_directStereo[1].data()};
	float* reflectedStereo[2] = {m_reflectedStereo[0].data(), m_reflectedStereo[1].data()};
	IPLAudioBuffer dryBuffer{1, static_cast<IPLint32>(kFrame), &dryPointer};
	IPLAudioBuffer directBuffer{1, static_cast<IPLint32>(kFrame), &directPointer};
	IPLAudioBuffer directStereoBuffer{2, static_cast<IPLint32>(kFrame), directStereo};
	IPLAudioBuffer reflectedStereoBuffer{2, static_cast<IPLint32>(kFrame), reflectedStereo};
	const IPLHRTF hrtf = SteamAudio::getInstance().getHRTF();

	//Direct: occlusion, transmission, distance and air, then put where the source is
	for (auto& channel : m_directStereo) channel.fill(0.0f);
	if (m_current.direct) {
		iplDirectEffectApply(m_directEffect, &m_current.directParams, &dryBuffer, &directBuffer);
		if (m_current.binaural) {
			IPLBinauralEffectParams params{m_current.direction, IPL_HRTFINTERPOLATION_BILINEAR, 1.0f, hrtf, nullptr};
			iplBinauralEffectApply(m_binauralEffect, &params, &directBuffer, &directStereoBuffer);
		}
		else {
			IPLPanningEffectParams params{m_current.direction};
			iplPanningEffectApply(m_panningEffect, &params, &directBuffer, &directStereoBuffer);
		}
	}

	//Reflections: the ambisonic convolution, then the decoder turns it round to where the listener's facing
	for (auto& channel : m_reflectedStereo) channel.fill(0.0f);
	const uint32_t order = m_current.reflectionOrder;
	const uint32_t channels = ReflectionField::channelsFor(order);
	std::array<float*, kChannels> ambisonics{};
	for (uint32_t c = 0; c < channels; ++c) {
		m_ambisonics[c].fill(0.0f);
		ambisonics[c] = m_ambisonics[c].data();
	}
	//Still fed while it's off so the filter history doesn't go stale, it just isn't heard
	m_reverb.process(m_dry.data(), kFrame, ambisonics.data(), channels);
	if (m_current.reflections) {
		IPLAudioBuffer ambisonicBuffer{static_cast<IPLint32>(channels), static_cast<IPLint32>(kFrame), ambisonics.data()};
		IPLAmbisonicsDecodeEffectParams params{static_cast<IPLint32>(order), hrtf, m_current.listener, m_current.binaural ? IPL_TRUE : IPL_FALSE};
		iplAmbisonicsDecodeEffectApply(m_decodeEffect, &params, &ambisonicBuffer, &reflectedStereoBuffer);
	}

	for (uint32_t i = 0; i < kFrame; ++i) {
		const float scale = kOutputGain * m_gainRamp[i];
		//Clamp instead of letting it wrap round into noise
		m_interleaved[i * 2] = std::clamp((m_directStereo[0][i] + m_reflectedStereo[0][i]) * scale, -1.0f, 1.0f);
		m_interleaved[i * 2 + 1] = std::clamp((m_directStereo[1][i] + m_reflectedStereo[1][i]) * scale, -1.0f, 1.0f);
	}
	m_reportedGain.store(m_gain, std::memory_order_relaxed);
	m_reportedOrder.store(m_current.reflections ? order : 0, std::memory_order_relaxed);
}

CraigError Craig::Audio::terminate() {
	stop();
	if (mp_stream) {
		//Closes the device too, and waits for a callback that's running before it goes
		SDL_DestroyAudioStream(mp_stream);
		mp_stream = nullptr;
	}
	m_reverb.stop();
	releaseEffects();
	mv_wave.clear();
	if (m_initialised) SDL_QuitSubSystem(SDL_INIT_AUDIO);
	m_initialised = false;
	return CRAIG_SUCCESS;
}
