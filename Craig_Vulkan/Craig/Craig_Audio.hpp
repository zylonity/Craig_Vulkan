#pragma once

#include "Craig_Constants.hpp"
#include "Craig_SoundProbe.hpp"
#include "Craig_Reverb.hpp"
#include <SDL3/SDL_audio.h>
#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace Craig {

	//What the game/editor can see about the audio, no SDL stuff
	struct AudioStatus {
		enum class State { Unavailable, Stopped, Paused, NoResult, Stale, Muted, Clear, Blocked, Partial };
		State state = State::Unavailable;
		bool available = false;
		bool muted = false;
		bool directEnabled = true;
		bool reflectionsEnabled = true;
		bool reverbReady = false;
		bool reverbFailed = false;
		float gain = 0.0f;
		uint32_t arrivals = 0; //How many the callback's actually playing right now
		//Timing from inside the callback, counted since the sound was opened
		//These are hints, not real dropout counts, SDL/pipewire don't tell us about underruns
		uint64_t callbackFrames = 0;
		float callbackMs = 0.0f;
		float callbackBudgetMs = 0.0f;
		float callbackPeakLoad = 0.0f;
		float callbackMaxGapMs = 0.0f;
		uint32_t callbackBufferFrames = 0;
		int callbackRate = 0;
		uint64_t callbackOverBudget = 0;
		uint64_t callbackLongGaps = 0;
		uint32_t reverbBanks = 0;
		uint64_t reverbChanges = 0;
		uint64_t reverbInvalidations = 0;
	};

	//Plays one sound from the source position, filtered by whatever the sound propagation found
	class Audio {
	public:
		~Audio() { terminate(); }
		CraigError init();
		CraigError playLoop(const std::string& path);
		//Loads it but doesn't play until playOneShot
		CraigError prepareOneShot(const std::string& path);
		//Each press adds another voice, ones already playing carry on
		void playOneShot();
		void stop();
		//Goes silent until the next valid result, scene swaps and pauses call this
		void invalidate();
		void update(const SoundProbeReadout& probe, bool worldRunning);
		void setMuted(bool muted);
		//For listening to what each part adds
		void setPathsEnabled(bool direct, bool reflections);
		AudioStatus getStatus() const;
		CraigError terminate();
		static const char* stateName(AudioStatus::State state);
		//Results older than this fade out instead of playing forever
		static constexpr uint64_t kMaxResultAgeMs = 250;

	private:
		struct Voice {
			double cursor = 0.0;
			bool active = false;
		};
		struct Tap {
			float delaySeconds;
			std::array<float, 3> gains;
			float left;
			float right;
		};
		struct ArrivalSet {
			bool audible = false;
			uint64_t deadlineMs = 0;
			uint32_t count = 0;
			std::array<Tap, kMaxSoundArrivals> taps{};
		};
		//The callback's own copy, with the delays already in samples for whatever rate the device is
		struct RenderSet {
			bool audible = false;
			uint64_t deadlineMs = 0;
			uint32_t count = 0;
			std::array<uint32_t, kMaxSoundArrivals> delays{};
			std::array<Tap, kMaxSoundArrivals> taps{};
		};

		//Triple buffer between the main thread (writes) and the audio callback (reads)
		//Whole sets go across without a lock, so the callback never has to wait on the main thread
		class Mailbox {
		public:
			ArrivalSet& back() { return m_buffers[m_back]; }
			void publish() { m_back = m_middle.exchange(m_back | kFresh, std::memory_order_acq_rel) & kIndex; }
			bool consume() {
				if (!(m_middle.load(std::memory_order_relaxed) & kFresh)) return false;
				m_front = m_middle.exchange(m_front, std::memory_order_acq_rel) & kIndex;
				return true;
			}
			const ArrivalSet& front() const { return m_buffers[m_front]; }
			void reset() { m_buffers = {}; m_back = 0; m_front = 2; m_middle.store(1, std::memory_order_relaxed); }
		private:
			static constexpr uint8_t kIndex = 3;
			static constexpr uint8_t kFresh = 4; //Set when the middle buffer hasn't been read yet
			std::array<ArrivalSet, 3> m_buffers{};
			uint8_t m_back = 0;
			uint8_t m_front = 2;
			std::atomic<uint8_t> m_middle{1};
		};

		static constexpr uint32_t kMaxOneShotVoices = 16;
		static constexpr uint64_t kShotCountMask = 0xffffffffull;
		static constexpr int kWaveRate = 48000;
		static constexpr float kOutputGain = 1.2f;
		static constexpr float kTransitionSeconds = 0.03f; //Crossfade between two arrival sets
		static constexpr float kSilenceFadeSeconds = 0.01f;
		static constexpr uint32_t kDelayCapacity = kSoundDelayCapacity; //Shared by the direct tap and the reflections' pre-delay
		CraigError loadWave(const std::string& path);
		CraigError openSound(const std::string& path, bool looping);
		void cancelOneShot();
		void publish(const SoundProbeReadout* probe, uint64_t deadlineMs);
		static void SDLCALL postmix(void* userdata, const SDL_AudioSpec* spec, float* buffer, int bytes);
		void mix(const SDL_AudioSpec& spec, float* buffer, int bytes, uint64_t nowMs);
		void recordCallback(uint64_t startNs, uint64_t endNs, uint32_t frames, int rate);
		void logDiagnostics(); //Once a second, main thread
		void renderTaps(const RenderSet& set, float weight, uint32_t writeIndex, float& left, float& right) const;

		bool m_initialised = false;
		SDL_AudioDeviceID m_device = 0;
		bool m_playing = false;
		bool m_muted = false;
		bool m_directEnabled = true;
		bool m_reflectionsEnabled = true;
		AudioStatus::State m_state = AudioStatus::State::Unavailable;
		Mailbox m_mailbox;
		Reverb m_reverb;
		bool m_reverbActive = false;
		uint64_t m_nextReverbUpdate = 0;
		uint64_t m_nextTimingLogMs = 0;
		uint64_t m_lastLoggedOverBudget = 0;
		//Callback writes, main thread reads, reset when a sound gets opened
		std::atomic<uint64_t> m_callbackFrames{0};
		std::atomic<uint64_t> m_callbackFormat{0}; //High word rate, low word buffer frames, so they can't tear
		std::atomic<uint64_t> m_callbackNs{0};
		std::atomic<float> m_callbackPeakLoad{0.0f};
		std::atomic<uint64_t> m_callbackOverBudget{0};
		std::atomic<uint64_t> m_callbackMaxGapNs{0};
		std::atomic<uint64_t> m_callbackLongGaps{0};
		std::atomic<float> m_reportedGain{0.0f};
		std::atomic<uint32_t> m_reportedArrivals{0};
		//Main thread writes it, callback reads it, both halves in one atomic so a cancel and a play can't tear
		//High word cancels a generation, low word counts presses so two quick presses don't merge into one
		std::atomic<uint64_t> m_playbackCommand{0};
		bool m_looping = true; //Doesn't change while the callback's registered

		//Everything below here belongs to the audio thread while the callback's registered
		std::vector<float> mv_wave;
		std::array<std::vector<float>, 3> mv_delayLines; //One per band
		uint64_t m_previousCallbackStartNs = 0;
		uint64_t m_previousCallbackBudgetNs = 0;
		uint32_t m_writeIndex = 0;
		uint64_t m_consumedPlaybackCommand = 0;
		std::array<Voice, kMaxOneShotVoices> m_voices{};
		uint32_t m_delayFrames = kDelayCapacity; //How much of the delay line is from this context, so a cancelled shot can't leak back out
		double m_cursor = 0.0;
		float m_lowSplit = 0.0f;
		float m_highSplit = 0.0f;
		float m_gain = 0.0f;
		float m_fade = 1.0f;
		RenderSet m_current;
		RenderSet m_previous;
	};

}
