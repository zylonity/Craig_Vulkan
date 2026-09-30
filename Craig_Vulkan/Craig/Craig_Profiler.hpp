#pragma once

#include "Craig_Constants.hpp"
#include <chrono>
#include <vector>


namespace Craig {

	// Simple CPU profiler, wrap a bit of code with CRAIG_PROFILE_SCOPE("name") and it prints
	// the average ms per frame of each section every second (turn it on with CRAIG_ENABLE_PROFILER)
	class Profiler {

	public:
		void addTime(const char* name, double ms);
		void endFrame();

		//===============================================================================
		// Singleton Implementations
		static Profiler& getInstance()
		{
			static Profiler instance; // Guaranteed to be destroyed.
			return instance;
		}
		// Make deleted functions public for nicer error messages (~ Scott Myers)
		Profiler(Profiler const&) = delete;		// Copy constructor
		void operator=(Profiler const&) = delete;	// Assignment Operator
		//===============================================================================
	private:

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		Profiler() : m_lastPrintTime(std::chrono::steady_clock::now()) {}	// Default Constructor private so can only be called from within
		//===============================================================================

		struct Section {
			const char* name;
			double totalMs;
		};

		std::vector<Section> mv_sections;
		uint32_t m_frameCount = 0;
		std::chrono::steady_clock::time_point m_lastPrintTime;
	};

	// times whatever scope it's made in and adds it to the profiler when it goes out of scope
	class ScopedTimer {

	public:
		explicit ScopedTimer(const char* name) : m_name(name), m_start(std::chrono::steady_clock::now()) {}
		~ScopedTimer();

	private:
		const char* m_name;
		std::chrono::steady_clock::time_point m_start;
	};

}

#if CRAIG_ENABLE_PROFILER
	#define CRAIG_PROFILE_CONCAT_INNER(a, b) a##b
	#define CRAIG_PROFILE_CONCAT(a, b) CRAIG_PROFILE_CONCAT_INNER(a, b)
	#define CRAIG_PROFILE_SCOPE(name) Craig::ScopedTimer CRAIG_PROFILE_CONCAT(craigProfileTimer_, __LINE__)(name)
	#define CRAIG_PROFILE_END_FRAME() Craig::Profiler::getInstance().endFrame()
#else
	#define CRAIG_PROFILE_SCOPE(name)
	#define CRAIG_PROFILE_END_FRAME()
#endif
