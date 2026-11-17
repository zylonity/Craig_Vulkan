#pragma once

#include "Craig_Constants.hpp"
#include <phonon.h>

namespace Craig {

	//The one Steam Audio context for the whole engine, the audio and both sound simulators share it
	//Valve Corporation, Apache-2.0 - Steam Audio 4.8.1, https://github.com/ValveSoftware/steam-audio
	class SteamAudio {

	public:
		//Every effect gets made for these, so the device stream asks SDL for the same rate
		static constexpr int kSamplingRate = 48000;
		static constexpr int kFrameSize = 256;
		//Third order is 16 channels, as far as the reflection effect and decoder go
		static constexpr int kMaxOrder = 3;
		static constexpr int kMaxChannels = (kMaxOrder + 1) * (kMaxOrder + 1);

		CraigError init();
		CraigError terminate();
		bool isReady() const { return m_context != nullptr; }
		IPLContext getContext() const { return m_context; }
		IPLHRTF getHRTF() const { return m_hrtf; }
		IPLAudioSettings getAudioSettings() const { return {kSamplingRate, kFrameSize}; }

		//===============================================================================
		// Singleton Implementations
		static SteamAudio& getInstance()
		{
			static SteamAudio instance; // Guaranteed to be destroyed.
			return instance;
		}
		// Make deleted functions public for nicer error messages (~ Scott Myers)
		SteamAudio(SteamAudio const&) = delete;	// Copy constructor
		void operator=(SteamAudio const&) = delete;	// Assignment Operator
		//===============================================================================
	private:

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		SteamAudio() {}										// Default Constructor private so can only be called from within
		//===============================================================================

		IPLContext m_context = nullptr;
		IPLHRTF m_hrtf = nullptr;
	};

	//Ambisonic channel count for an order, (order + 1)^2
	inline constexpr int ambisonicChannels(int order) { return (order + 1) * (order + 1); }

}
