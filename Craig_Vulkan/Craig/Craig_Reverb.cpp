#include "Craig_Reverb.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "SteamAudio/reconstructor.h"

//The callback reads all of these, none of them can be allowed to lock
static_assert(std::atomic<uint8_t>::is_always_lock_free);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

std::array<std::vector<float>, 2> Craig::Reverb::reconstruct(const ReverbEnergy& energy, int rate, float directDelaySeconds) {
	if (rate < 8000 || rate > 192000) throw std::runtime_error("unsupported reverb sample rate");
	if (!std::isfinite(directDelaySeconds) || directDelaySeconds < 0.0f) throw std::runtime_error("invalid reflection pre-delay");
	constexpr float duration = kReverbBins * kReverbBinSeconds;

	//Copy our bins into Steam Audio's energy field, order 0 so just the one channel
	ipl::EnergyField field(duration, 0);
	for (uint32_t bin = 0; bin < kReverbBins; ++bin) {
		for (uint32_t band = 0; band < 3; ++band) {
			const float e = energy[bin][band];
			if (!std::isfinite(e) || e < 0.0f) throw std::runtime_error("invalid reflection energy");
			field[0][band][bin] = e;
		}
	}
	ipl::ImpulseResponse response(duration, 0, rate);
	ipl::Reconstructor reconstructor(duration, 0, rate);
	const ipl::EnergyField* fields[] = {&field};
	const float* corrections[] = {nullptr};
	ipl::AirAbsorptionModel air;
	ipl::ImpulseResponse* responses[] = {&response};
	reconstructor.reconstruct(1, fields, corrections, &air, responses, ipl::ReconstructionType::Linear, duration, 0);

	//Upstream's IR starts at the direct sound, so put the travel time back once, capped the same as the direct tap's delay
	const size_t preDelay = static_cast<size_t>(std::min(std::round(static_cast<double>(directDelaySeconds) * rate),
		static_cast<double>(kSoundDelayCapacity - 1)));
	std::array<std::vector<float>, 2> result;
	for (auto& channel : result) {
		channel.assign(preDelay + response.numSamples(), 0.0f);
		//Order 0 is the same in both ears, it's a centred mono preview, not proper directional/HRTF stuff
		for (int i = 0; i < response.numSamples(); ++i) channel[preDelay + i] = response[0][i] * std::sqrt(0.5f);
	}
	return result;
}

void Craig::Reverb::start() {
	stop();
	m_stopping = false;
	m_failed.store(false, std::memory_order_relaxed);
	m_worker = std::thread(&Reverb::work, this);
}

void Craig::Reverb::stop() {
	{
		std::lock_guard lock(m_mutex);
		m_stopping = true;
		m_pending.reset();
	}
	m_wake.notify_one();
	if (m_worker.joinable()) m_worker.join();
	for (auto& bank : m_banks) {
		for (auto& filter : bank.filters) filter.reset();
		bank.state.store(Free, std::memory_order_relaxed);
	}
	m_current = m_previous = -1;
	m_fade = 1.0f;
	m_drainFrames = 0;
	m_lastSubmitted.reset();
	m_ready.store(false, std::memory_order_relaxed);
}

void Craig::Reverb::invalidate() {
	m_epoch.fetch_add(1, std::memory_order_release);
	m_ready.store(false, std::memory_order_relaxed);
	std::lock_guard lock(m_mutex);
	m_pending.reset();
	m_lastSubmitted.reset();
}

void Craig::Reverb::submit(const ReverbEnergy& energy, float directDelaySeconds) {
	Job job{energy, m_epoch.load(std::memory_order_acquire), m_rate.load(std::memory_order_relaxed), directDelaySeconds};
	std::lock_guard lock(m_mutex);
	if (!m_worker.joinable() || m_stopping) return;
	//If nothing's moving don't keep rebuilding the same filter, that restarts its delay history every time
	if (m_lastSubmitted && job.epoch == m_lastSubmitted->epoch && job.rate == m_lastSubmitted->rate &&
		job.directDelaySeconds == m_lastSubmitted->directDelaySeconds && job.energy == m_lastSubmitted->energy) return;
	m_lastSubmitted = job;
	m_pending = job;
	m_wake.notify_one();
}

void Craig::Reverb::work() {
	for (;;) {
		Job job;
		{
			std::unique_lock lock(m_mutex);
			m_wake.wait(lock, [this] { return m_stopping || m_pending.has_value(); });
			if (m_stopping) return;
			job = *m_pending;
			m_pending.reset();
		}

		Bank* target = nullptr;
		for (auto& bank : m_banks) {
			uint8_t expected = Free;
			if (bank.state.compare_exchange_strong(expected, Building, std::memory_order_acq_rel)) { target = &bank; break; }
		}
		if (!target) {
			//An old tail's still draining, so replace the one that's queued instead of eventually playing an out of date one
			for (auto& bank : m_banks) {
				uint8_t expected = Ready;
				if (bank.state.compare_exchange_strong(expected, Building, std::memory_order_acq_rel)) { target = &bank; break; }
			}
		}
		if (!target) {
			//Don't wait for the callback, just let the main thread submit it again later
			std::lock_guard lock(m_mutex);
			m_lastSubmitted.reset();
			continue;
		}

		try {
			const auto ir = reconstruct(job.energy, job.rate, job.directDelaySeconds);
			for (uint32_t i = 0; i < 2; ++i) {
				if (!target->filters[i].init(128, 2048, ir[i].data(), ir[i].size())) throw std::runtime_error("convolver initialisation failed");
			}
			target->responseFrames = static_cast<uint32_t>(ir[0].size());
			target->epoch = job.epoch;
			target->rate = job.rate;
			//Invalidated while we were building it, don't hand it over
			target->state.store(job.epoch == m_epoch.load(std::memory_order_acquire) ? Ready : Free, std::memory_order_release);
		}
		catch (...) {
			m_failed.store(true, std::memory_order_relaxed);
			target->state.store(Free, std::memory_order_release);
		}
	}
}

void Craig::Reverb::processSample(float input, int rate, float& left, float& right) {
	m_rate.store(rate, std::memory_order_relaxed);
	const uint32_t epoch = m_epoch.load(std::memory_order_acquire);

	//Drop anything from an old epoch or for a different sample rate
	for (int* index : {&m_current, &m_previous}) {
		if (*index >= 0 && (m_banks[*index].epoch != epoch || m_banks[*index].rate != rate)) {
			m_banks[*index].state.store(Free, std::memory_order_release);
			*index = -1;
		}
	}

	//Only two at once, so a new one waits until the previous tail's finished draining
	for (int i = 0; i < 3; ++i) {
		auto& bank = m_banks[i];
		if (m_previous >= 0) break;
		//Claim it before reading anything, the worker's allowed to replace one that's only Ready
		uint8_t expected = Ready;
		if (!bank.state.compare_exchange_strong(expected, Active, std::memory_order_acq_rel)) continue;
		if (bank.epoch != epoch || bank.rate != rate) { bank.state.store(Free, std::memory_order_release); continue; }
		m_previous = m_current;
		m_current = i;
		m_fade = m_previous >= 0 ? 0.0f : 1.0f;
		m_drainFrames = m_previous >= 0 ? m_banks[m_previous].responseFrames + static_cast<uint32_t>(std::ceil(0.05 * rate)) : 0;
		break;
	}
	m_ready.store(m_current >= 0, std::memory_order_relaxed);

	for (int pass = 0; pass < 2; ++pass) {
		const int index = pass == 0 ? m_current : m_previous;
		if (index < 0) continue;
		const float weight = pass == 0 ? m_fade : 1.0f - m_fade;
		float l = 0.0f, r = 0.0f;
		//Crossfade the sound going IN, not what comes out, the old room still owes us the echoes of what it already got
		const float sample = weight * input;
		m_banks[index].filters[0].process(&sample, &l, 1);
		m_banks[index].filters[1].process(&sample, &r, 1);
		left += l;
		right += r;
	}
	m_fade = std::min(1.0f, m_fade + 1.0f / (0.05f * rate));
	if (m_previous >= 0 && m_drainFrames > 0 && --m_drainFrames == 0) {
		m_banks[m_previous].state.store(Free, std::memory_order_release);
		m_previous = -1;
	}
}
