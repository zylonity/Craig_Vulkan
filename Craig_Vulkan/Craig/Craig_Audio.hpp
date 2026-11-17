#pragma once

#include "Craig_Constants.hpp"
#include "Craig_SoundProbe.hpp"
#include "Craig_Reverb.hpp"
#include "Craig_SteamAudio.hpp"
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
		bool pathingEnabled = true;
		bool binaural = true;
		bool reverbReady = false;
		bool reverbFailed = false;
		float gain = 0.0f;
		uint32_t reflectionOrder = 0; //What the decoder's running at right now
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

	//Plays one sound from the source position through Steam Audio's effects, driven by whatever the sound propagation found
	//Direct: direct effect then HRTF. Reflections: ambisonic convolution then the HRTF decoder
	class Audio {
	public:
		~Audio() { terminate(); }
		CraigError init();
		CraigError playLoop(const std::string& path);
		//Loads it but doesn't play until playOneShot
		CraigError prepareOneShot(const std::string& path);
		//Each press adds another voice, ones already playing carry on
		void playOneShot(float gain = 1.0f);
		void stop();
		//Goes silent until the next valid result, scene swaps and pauses call this
		void invalidate();
		void update(const SoundProbeReadout& probe, bool worldRunning);
		void setMuted(bool muted);
		//For listening to what each part adds
		void setPathsEnabled(bool direct, bool reflections);
		//Off pans with Steam Audio's speaker panning instead, to hear what the HRTF adds
		void setBinaural(bool binaural) { m_binaural = binaural; }
		void setPathingEnabled(bool pathing) { m_pathingEnabled = pathing; }
		AudioStatus getStatus() const;
		CraigError terminate();
		static const char* stateName(AudioStatus::State state);
		//Results older than this fade out instead of playing forever
		static constexpr uint64_t kMaxResultAgeMs = 250;

	private:
		struct Voice {
			double cursor = 0.0;
			bool active = false;
			float gain = 1.0f;
		};
		//Everything the callback needs for one result, copied across whole
		struct ParameterSet {
			bool audible = false;
			uint64_t deadlineMs = 0;
			bool direct = true;
			bool reflections = true;
			bool binaural = true;
			uint32_t reflectionOrder = 0;
			IPLDirectEffectParams directParams{};
			IPLVector3 direction{0.0f, 0.0f, -1.0f}; //Listener space, for the HRTF
			IPLCoordinateSpace3 listener{};
			//The CPU simulator's own response, handed straight back to the library's reflection effect
			bool steamReflections = false;
			IPLReflectionEffectParams steamReflectionParams{};
			//Sound that bent round to the listener, only heard as much as the direct path is blocked
			bool pathing = false;
			float pathingMix = 0.0f;
			uint32_t pathingOrder = 1;
			std::array<float, 3> pathingEQ{1.0f, 1.0f, 1.0f};
			std::array<float, 16> pathingSH{};
		};

		//Triple buffer between the main thread (writes) and the audio callback (reads)
		//Whole sets go across without a lock, so the callback never has to wait on the main thread
		class Mailbox {
		public:
			ParameterSet& back() { return m_buffers[m_back]; }
			void publish() { m_back = m_middle.exchange(m_back | kFresh, std::memory_order_acq_rel) & kIndex; }
			bool consume() {
				if (!(m_middle.load(std::memory_order_relaxed) & kFresh)) return false;
				m_front = m_middle.exchange(m_front, std::memory_order_acq_rel) & kIndex;
				return true;
			}
			const ParameterSet& front() const { return m_buffers[m_front]; }
			void reset() { m_buffers = {}; m_back = 0; m_front = 2; m_middle.store(1, std::memory_order_relaxed); }
		private:
			static constexpr uint8_t kIndex = 3;
			static constexpr uint8_t kFresh = 4; //Set when the middle buffer hasn't been read yet
			std::array<ParameterSet, 3> m_buffers{};
			uint8_t m_back = 0;
			uint8_t m_front = 2;
			std::atomic<uint8_t> m_middle{1};
		};

		static constexpr uint32_t kMaxOneShotVoices = 16;
		static constexpr uint64_t kShotCountMask = 0x00ffffffull;
		static constexpr uint64_t kShotGenerationMask = 0xffffffff00000000ull;
		static constexpr int kRate = SteamAudio::kSamplingRate;
		static constexpr uint32_t kFrame = SteamAudio::kFrameSize;
		static constexpr uint32_t kChannels = SteamAudio::kMaxChannels;
		static constexpr float kOutputGain = 1.2f;
		static constexpr float kRiseSeconds = 0.03f;
		static constexpr float kSilenceFadeSeconds = 0.01f;
		CraigError loadWave(const std::string& path);
		CraigError openSound(const std::string& path, bool looping);
		CraigError createEffects();
		void releaseEffects();
		void cancelOneShot();
		void publish(const SoundProbeReadout* probe, uint64_t deadlineMs);
		static void SDLCALL streamCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount);
		void renderBlock(uint64_t nowMs);
		void renderDry(bool audible);
		void recordCallback(uint64_t startNs, uint64_t endNs, uint32_t frames);
		void logDiagnostics(); //Once a second, main thread

		bool m_initialised = false;
		SDL_AudioStream* mp_stream = nullptr;
		bool m_playing = false;
		bool m_muted = false;
		bool m_directEnabled = true;
		bool m_reflectionsEnabled = true;
		bool m_binaural = true;
		bool m_pathingEnabled = true;
		AudioStatus::State m_state = AudioStatus::State::Unavailable;
		Mailbox m_mailbox;
		Reverb m_reverb;
		bool m_reverbActive = false;
		bool m_steamReflectionsActive = false;
		uint32_t m_reverbOrder = 0;
		uint64_t m_nextReverbUpdate = 0;
		uint64_t m_nextTimingLogMs = 0;
		uint64_t m_lastLoggedOverBudget = 0;
		//Callback writes, main thread reads, reset when a sound gets opened
		std::atomic<uint64_t> m_callbackFrames{0};
		std::atomic<uint32_t> m_callbackBufferFrames{0};
		std::atomic<uint64_t> m_callbackNs{0};
		std::atomic<float> m_callbackPeakLoad{0.0f};
		std::atomic<uint64_t> m_callbackOverBudget{0};
		std::atomic<uint64_t> m_callbackMaxGapNs{0};
		std::atomic<uint64_t> m_callbackLongGaps{0};
		std::atomic<float> m_reportedGain{0.0f};
		std::atomic<uint32_t> m_reportedOrder{0};
		//Main thread writes it, callback reads it, both halves in one atomic so a cancel and a play can't tear
		//High word cancels a generation, low word packs an 8-bit gain and 24-bit press count
		std::atomic<uint64_t> m_playbackCommand{0};
		bool m_looping = true; //Doesn't change while the callback's registered

		//Steam Audio effects, made before the stream starts and only touched by the callback after that
		IPLDirectEffect m_directEffect = nullptr;
		IPLBinauralEffect m_binauralEffect = nullptr;
		IPLPanningEffect m_panningEffect = nullptr;
		IPLAmbisonicsDecodeEffect m_decodeEffect = nullptr;
		IPLReflectionEffect m_reflectionEffect = nullptr;
		IPLPathEffect m_pathEffect = nullptr;
		float m_pathingMix = 0.0f; //Callback only, ramped across each block so the mix never jumps
		bool m_steamReflectionsPlaying = false; //Callback only, a reflection effect starting up again shouldn't play an old tail

		//Everything below here belongs to the audio thread while the callback's registered
		std::vector<float> mv_wave;
		std::array<float, kFrame> m_dry{};
		std::array<float, kFrame> m_directMono{};
		std::array<float, kFrame> m_gainRamp{};
		std::array<std::array<float, kFrame>, 2> m_directStereo{};
		std::array<std::array<float, kFrame>, 2> m_reflectedStereo{};
		std::array<std::array<float, kFrame>, 2> m_pathStereo{};
		std::array<std::array<float, kFrame>, kChannels> m_ambisonics{};
		std::array<float, kFrame * 2> m_interleaved{};
		uint64_t m_previousCallbackStartNs = 0;
		uint64_t m_previousCallbackBudgetNs = 0;
		uint64_t m_consumedPlaybackCommand = 0;
		std::array<Voice, kMaxOneShotVoices> m_voices{};
		double m_cursor = 0.0;
		float m_gain = 0.0f;
		bool m_resetEffects = false; //A cancel happened, so whatever's still ringing in the effects goes
		ParameterSet m_current;
	};

}
