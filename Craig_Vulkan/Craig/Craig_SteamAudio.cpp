#include "Craig_SteamAudio.hpp"
#include "Craig_Logger.hpp"

namespace {
	//Steam Audio prints to stdout otherwise, this way it ends up in the log window too
	void IPLCALL logSteamAudio(IPLLogLevel level, const char* message) {
		switch (level) {
			case IPL_LOGLEVEL_ERROR: Craig::Logger::audio().error("Steam Audio: {}", message); break;
			case IPL_LOGLEVEL_WARNING: Craig::Logger::audio().warn("Steam Audio: {}", message); break;
			case IPL_LOGLEVEL_DEBUG: Craig::Logger::audio().debug("Steam Audio: {}", message); break;
			default: Craig::Logger::audio().info("Steam Audio: {}", message); break;
		}
	}
}

CraigError Craig::SteamAudio::init() {
	if (m_context) return CRAIG_SUCCESS;

	IPLContextSettings contextSettings{};
	contextSettings.version = STEAMAUDIO_VERSION;
	contextSettings.logCallback = logSteamAudio;
	contextSettings.simdLevel = IPL_SIMDLEVEL_AVX2; //AVX512 can throttle the clocks, upstream suggests capping it
	if (iplContextCreate(&contextSettings, &m_context) != IPL_STATUS_SUCCESS) {
		Logger::audio().error("Couldn't create a Steam Audio context, the library might be the wrong version");
		m_context = nullptr;
		return CRAIG_FAIL;
	}

	//The built in HRTF, every binaural effect and the decoder use the same one
	IPLAudioSettings audioSettings = getAudioSettings();
	IPLHRTFSettings hrtfSettings{};
	hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;
	hrtfSettings.volume = 1.0f;
	hrtfSettings.normType = IPL_HRTFNORMTYPE_NONE;
	if (iplHRTFCreate(m_context, &audioSettings, &hrtfSettings, &m_hrtf) != IPL_STATUS_SUCCESS) {
		Logger::audio().error("Couldn't create Steam Audio's default HRTF");
		terminate();
		return CRAIG_FAIL;
	}

	Logger::audio().info("Steam Audio {}.{}.{} ready ({} Hz, {} sample frames)", STEAMAUDIO_VERSION_MAJOR, STEAMAUDIO_VERSION_MINOR,
		STEAMAUDIO_VERSION_PATCH, kSamplingRate, kFrameSize);
	return CRAIG_SUCCESS;
}

CraigError Craig::SteamAudio::terminate() {
	//Everything else made from the context has to be released before this
	if (m_hrtf) iplHRTFRelease(&m_hrtf);
	if (m_context) iplContextRelease(&m_context);
	m_hrtf = nullptr;
	m_context = nullptr;
	return CRAIG_SUCCESS;
}
