#include "Craig_Reverb.hpp"
#include "Craig_Logger.hpp"
#include "Craig_SteamAudio.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>

//The callback reads all of these, none of them can be allowed to lock
static_assert(std::atomic<uint8_t>::is_always_lock_free);
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

namespace {
	constexpr float kDuration = Craig::kReverbBins * Craig::kReverbBinSeconds;

	//The library objects one reconstruction needs, made once for the biggest order so the worker can reuse them
	struct Reconstruction {
		IPLEnergyField field = nullptr;
		IPLImpulseResponse response = nullptr;
		IPLReconstructor reconstructor = nullptr;

		Reconstruction() {
			const IPLContext context = Craig::SteamAudio::getInstance().getContext();
			if (!context) throw std::runtime_error("Steam Audio isn't initialised");
			const int order = static_cast<int>(Craig::kMaxReflectionOrder);
			IPLEnergyFieldSettings fieldSettings{kDuration, order};
			IPLImpulseResponseSettings responseSettings{kDuration, order, Craig::SteamAudio::kSamplingRate};
			IPLReconstructorSettings reconstructorSettings{kDuration, order, Craig::SteamAudio::kSamplingRate};
			if (iplEnergyFieldCreate(context, &fieldSettings, &field) != IPL_STATUS_SUCCESS ||
				iplImpulseResponseCreate(context, &responseSettings, &response) != IPL_STATUS_SUCCESS ||
				iplReconstructorCreate(context, &reconstructorSettings, &reconstructor) != IPL_STATUS_SUCCESS) {
				release();
				throw std::runtime_error("couldn't allocate the reflection reconstruction");
			}
		}
		~Reconstruction() { release(); }
		Reconstruction(const Reconstruction&) = delete;
		Reconstruction& operator=(const Reconstruction&) = delete;

		void release() {
			if (field) iplEnergyFieldRelease(&field);
			if (response) iplImpulseResponseRelease(&response);
			if (reconstructor) iplReconstructorRelease(&reconstructor);
		}

		//Same Linear reconstruction with the default air model that the old vendored copy did
		std::vector<std::vector<float>> run(const Craig::ReflectionField& input) {
			if (input.order > Craig::kMaxReflectionOrder) throw std::runtime_error("reflection order too high");
			const uint32_t channels = Craig::ReflectionField::channelsFor(input.order);
			if (input.energy.size() != channels * Craig::kReverbBands * Craig::kReverbBins) throw std::runtime_error("reflection field is the wrong size");
			//Higher channels than this order hold the last job's data, the reconstruction only reads up to the order asked for
			iplEnergyFieldReset(field);
			for (uint32_t channel = 0; channel < channels; ++channel) {
				for (uint32_t band = 0; band < Craig::kReverbBands; ++band) {
					float* bins = iplEnergyFieldGetBand(field, static_cast<IPLint32>(channel), static_cast<IPLint32>(band));
					for (uint32_t bin = 0; bin < Craig::kReverbBins; ++bin) {
						const float e = input.at(channel, band, bin);
						//The higher channels are signed, only the omni one has to be positive
						if (!std::isfinite(e) || (channel == 0 && e < 0.0f)) throw std::runtime_error("invalid reflection energy");
						bins[bin] = e;
					}
				}
			}
			IPLReconstructorInputs inputs{field};
			IPLReconstructorSharedInputs shared{kDuration, static_cast<IPLint32>(input.order)};
			IPLReconstructorOutputs outputs{response};
			iplReconstructorReconstruct(reconstructor, 1, &inputs, &shared, &outputs);

			const int samples = iplImpulseResponseGetNumSamples(response);
			std::vector<std::vector<float>> result(channels);
			for (uint32_t channel = 0; channel < channels; ++channel) {
				const float* data = iplImpulseResponseGetChannel(response, static_cast<int>(channel));
				result[channel].assign(data, data + samples);
			}
			return result;
		}
	};

	//Dumps what the last prepared tail was made from, so I can plot it and see where a gap or spike came from
	//Overwritten every time, it's not necessarily the one that's playing yet
	CraigError saveTailSnapshot(const Craig::ReflectionField& field, const std::vector<std::vector<float>>& ir, uint32_t epoch) {
		constexpr const char* path = "logs/reverb-energy.csv";
		std::ofstream file(path);
		if (!file) {
			Craig::Logger::audio().warn("Couldn't open reverb diagnostic snapshot {}", path);
			return CRAIG_FAIL;
		}
		file << std::setprecision(std::numeric_limits<double>::max_digits10);
		double irEnergy = 0.0;
		for (const auto& channel : ir) for (float sample : channel) irEnergy += static_cast<double>(sample) * sample;
		file << "# prepared response, not necessarily active; Steam Audio 4.8.1 SH energy, direct-relative bins, omni channel below\n"
			<< "# epoch=" << epoch << ",sample_rate=" << Craig::SteamAudio::kSamplingRate << ",order=" << field.order
			<< ",ir_frames=" << (ir.empty() ? 0 : ir[0].size()) << ",ir_energy_all_channels=" << irEnergy << '\n'
			<< "bin,relative_start_seconds,low,mid,high\n";
		for (uint32_t bin = 0; bin < Craig::kReverbBins; ++bin) {
			file << bin << ',' << static_cast<double>(bin) * Craig::kReverbBinSeconds;
			for (uint32_t band = 0; band < Craig::kReverbBands; ++band) file << ',' << field.at(0, band, bin);
			file << '\n';
		}
		file.close();
		if (!file) {
			Craig::Logger::audio().warn("Couldn't finish reverb diagnostic snapshot {}", path);
			return CRAIG_FAIL;
		}
		Craig::Logger::audio().info("Prepared Steam Audio reflection snapshot: {} (epoch {}, order {})", path, epoch, field.order);
		return CRAIG_SUCCESS;
	}
}

std::vector<std::vector<float>> Craig::Reverb::reconstruct(const ReflectionField& field) {
	Reconstruction reconstruction;
	return reconstruction.run(field);
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
	m_activeBanks.store(0, std::memory_order_relaxed);
	m_changes.store(0, std::memory_order_relaxed);
	m_invalidations.store(0, std::memory_order_relaxed);
}

void Craig::Reverb::invalidate() {
	m_invalidations.fetch_add(1, std::memory_order_relaxed);
	m_epoch.fetch_add(1, std::memory_order_release);
	m_ready.store(false, std::memory_order_relaxed);
	std::lock_guard lock(m_mutex);
	m_pending.reset();
	m_lastSubmitted.reset();
}

void Craig::Reverb::submit(std::shared_ptr<const ReflectionField> field) {
	if (!field) return;
	Job job{std::move(field), m_epoch.load(std::memory_order_acquire)};
	std::lock_guard lock(m_mutex);
	if (!m_worker.joinable() || m_stopping) return;
	//If nothing's moving don't keep rebuilding the same filter, that restarts its delay history every time
	if (m_lastSubmitted && job.epoch == m_lastSubmitted->epoch && job.field->order == m_lastSubmitted->field->order &&
		job.field->energy == m_lastSubmitted->field->energy) return;
	m_lastSubmitted = job;
	m_pending = job;
	m_wake.notify_one();
}

void Craig::Reverb::work() {
	//Made on this thread and only used here, the library objects aren't shared with anything
	std::optional<Reconstruction> reconstruction;
	try {
		reconstruction.emplace();
	}
	catch (const std::exception& error) {
		Logger::audio().error("Reflection reconstruction unavailable: {}", error.what());
		m_failed.store(true, std::memory_order_relaxed);
		return;
	}

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
			const auto ir = reconstruction->run(*job.field);
			for (size_t i = 0; i < ir.size(); ++i) {
				if (!target->filters[i].init(128, 2048, ir[i].data(), ir[i].size())) throw std::runtime_error("convolver initialisation failed");
			}
			//Written here on the worker, never in the callback
			if (job.epoch == m_epoch.load(std::memory_order_acquire)) saveTailSnapshot(*job.field, ir, job.epoch);
			target->channels = static_cast<uint32_t>(ir.size());
			target->responseFrames = static_cast<uint32_t>(ir[0].size());
			target->epoch = job.epoch;
			//Invalidated while we were building it, don't hand it over
			target->state.store(job.epoch == m_epoch.load(std::memory_order_acquire) ? Ready : Free, std::memory_order_release);
		}
		catch (const std::exception& error) {
			Logger::audio().error("Reflection filter failed: {}", error.what());
			m_failed.store(true, std::memory_order_relaxed);
			target->state.store(Free, std::memory_order_release);
		}
	}
}

void Craig::Reverb::process(const float* input, uint32_t frames, float* const* out, uint32_t channels) {
	frames = std::min(frames, kMaxFrames);
	const uint32_t epoch = m_epoch.load(std::memory_order_acquire);

	//Drop anything from an old epoch
	for (int* index : {&m_current, &m_previous}) {
		if (*index >= 0 && m_banks[*index].epoch != epoch) {
			m_banks[*index].state.store(Free, std::memory_order_release);
			*index = -1;
			m_activeBanks.store((m_current >= 0) + (m_previous >= 0), std::memory_order_relaxed);
		}
	}

	//Only two at once, so a new one waits until the previous tail's finished draining
	for (int i = 0; i < 3; ++i) {
		auto& bank = m_banks[i];
		if (m_previous >= 0) break;
		//Claim it before reading anything, the worker's allowed to replace one that's only Ready
		uint8_t expected = Ready;
		if (!bank.state.compare_exchange_strong(expected, Active, std::memory_order_acq_rel)) continue;
		if (bank.epoch != epoch) { bank.state.store(Free, std::memory_order_release); continue; }
		m_previous = m_current;
		m_current = i;
		m_changes.fetch_add(1, std::memory_order_relaxed);
		m_activeBanks.store((m_current >= 0) + (m_previous >= 0), std::memory_order_relaxed);
		m_fade = m_previous >= 0 ? 0.0f : 1.0f;
		m_drainFrames = m_previous >= 0 ?
			m_banks[m_previous].responseFrames + static_cast<uint32_t>(std::ceil(0.05 * SteamAudio::kSamplingRate)) : 0;
		break;
	}
	m_ready.store(m_current >= 0, std::memory_order_relaxed);

	const float fadeStep = 1.0f / (0.05f * SteamAudio::kSamplingRate);
	for (int pass = 0; pass < 2; ++pass) {
		const int index = pass == 0 ? m_current : m_previous;
		if (index < 0) continue;
		//Crossfade the sound going IN, not what comes out, the old room still owes us the echoes of what it already got
		for (uint32_t i = 0; i < frames; ++i) {
			const float fade = std::min(1.0f, m_fade + fadeStep * i);
			m_weighted[i] = (pass == 0 ? fade : 1.0f - fade) * input[i];
		}
		Bank& bank = m_banks[index];
		for (uint32_t channel = 0; channel < bank.channels; ++channel) {
			bank.filters[channel].process(m_weighted.data(), m_filtered.data(), frames);
			if (channel >= channels) continue;
			for (uint32_t i = 0; i < frames; ++i) out[channel][i] += m_filtered[i];
		}
	}
	m_fade = std::min(1.0f, m_fade + fadeStep * frames);
	if (m_previous >= 0 && m_drainFrames > 0) {
		m_drainFrames -= std::min(m_drainFrames, frames);
		if (m_drainFrames == 0) {
			m_banks[m_previous].state.store(Free, std::memory_order_release);
			m_previous = -1;
			m_activeBanks.store((m_current >= 0) + (m_previous >= 0), std::memory_order_relaxed);
		}
	}
}
