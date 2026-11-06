#pragma once

#include "Craig_SoundProbe.hpp"
#include "FFTConvolver/TwoStageFFTConvolver.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace Craig {

	//Turns the GPU's reflection energy into an impulse response and convolves the sound with it
	//The reconstruction is Valve's Steam Audio 4.8.1 (Apache-2.0), vendored in External/SteamAudio - https://github.com/ValveSoftware/steam-audio
	//This class just owns when filters get built and swapped, so the audio callback never builds one itself
	class Reverb {
	public:
		~Reverb() { stop(); }
		void start();
		void stop(); //Only once SDL's taken the callback off
		void invalidate();
		void submit(const ReverbEnergy& energy, float directDelaySeconds = 0.0f); //Main thread, never the callback
		void processSample(float input, int rate, float& left, float& right);
		bool ready() const { return m_ready.load(std::memory_order_relaxed); }
		bool failed() const { return m_failed.load(std::memory_order_relaxed); }
		//Just for the timing log/demo window
		uint32_t activeBanks() const { return m_activeBanks.load(std::memory_order_relaxed); }
		uint64_t changes() const { return m_changes.load(std::memory_order_relaxed); }
		uint64_t invalidations() const { return m_invalidations.load(std::memory_order_relaxed); }
		static std::array<std::vector<float>, 2> reconstruct(const ReverbEnergy& energy, int rate, float directDelaySeconds = 0.0f);

	private:
		//Free -> Building (worker) -> Ready -> Active (callback) -> Free
		enum State : uint8_t { Free, Building, Ready, Active };
		struct Bank {
			std::atomic<uint8_t> state{Free};
			uint32_t epoch = 0;
			int rate = 0;
			uint32_t responseFrames = 0;
			std::array<fftconvolver::TwoStageFFTConvolver, 2> filters; //Left and right
		};
		struct Job {
			ReverbEnergy energy;
			uint32_t epoch;
			int rate;
			float directDelaySeconds;
		};
		void work();
		std::array<Bank, 3> m_banks;
		std::atomic<uint32_t> m_epoch{1}; //Goes up on invalidate, anything built for an older one gets thrown away
		std::atomic<int> m_rate{48000};
		std::atomic<bool> m_ready{false};
		std::atomic<bool> m_failed{false};
		std::atomic<uint32_t> m_activeBanks{0};
		std::atomic<uint64_t> m_changes{0};
		std::atomic<uint64_t> m_invalidations{0};
		std::thread m_worker;
		std::mutex m_mutex;
		std::condition_variable m_wake;
		bool m_stopping = false;
		std::optional<Job> m_pending;
		std::optional<Job> m_lastSubmitted;
		//Callback only, the worker only ever touches banks that are Building
		int m_current = -1;
		int m_previous = -1;
		float m_fade = 1.0f;
		uint32_t m_drainFrames = 0; //The old filter keeps going until everything it was given has come back out
	};
}
