#pragma once

#include "Craig_Constants.hpp"
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>


namespace Craig {

	// One line in the editor's log window
	struct LogEntry {
		spdlog::level::level_enum level;
		std::string text;
	};

	// spdlog sink that keeps the last kMaxEditorLogLines lines in memory for the editor to draw
	class EditorLogSink : public spdlog::sinks::base_sink<std::mutex> {

	public:
		std::vector<LogEntry> getEntries();
		uint64_t getVersion(); // goes up whenever a line's added or cleared
		void clear();

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override;
		void flush_() override {} // nothing to flush, it's all in memory

	private:
		std::deque<LogEntry> m_entries;
		uint64_t m_version = 0;
	};

	// Sets up spdlog so everything goes to the console, logs/Craig.log and the editor's log window
	// log with Craig::Logger::renderer().warn("Something broke: {}", reason)
	// {} gets swapped for the arguments
	class Logger {

	public:
		CraigError init();
		CraigError terminate();

		std::shared_ptr<EditorLogSink> getEditorSink() { return mp_editorSink; };

		// one logger per system so you can tell who's complaining
		static spdlog::logger& engine() { return get(getInstance().mp_engine); };
		static spdlog::logger& renderer() { return get(getInstance().mp_renderer); };
		static spdlog::logger& vulkan() { return get(getInstance().mp_vulkan); };
		static spdlog::logger& physics() { return get(getInstance().mp_physics); };
		static spdlog::logger& scene() { return get(getInstance().mp_scene); };
		static spdlog::logger& resources() { return get(getInstance().mp_resources); };

		//===============================================================================
		// Singleton Implementations
		static Logger& getInstance()
		{
			static Logger instance; // Guaranteed to be destroyed.
			return instance;
		}
		// Make deleted functions public for nicer error messages (~ Scott Myers)
		Logger(Logger const&) = delete;		// Copy constructor
		void operator=(Logger const&) = delete;	// Assignment Operator
		//===============================================================================
	private:

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		Logger() {}	// Default Constructor private so can only be called from within
		//===============================================================================

		// Falls back to spdlog's default console logger if something logs before init()
		static spdlog::logger& get(const std::shared_ptr<spdlog::logger>& pLogger);
		std::shared_ptr<spdlog::logger> createLogger(const char* name);

		std::vector<spdlog::sink_ptr> mv_sinks;
		std::shared_ptr<EditorLogSink> mp_editorSink;

		std::shared_ptr<spdlog::logger> mp_engine;
		std::shared_ptr<spdlog::logger> mp_renderer;
		std::shared_ptr<spdlog::logger> mp_vulkan;
		std::shared_ptr<spdlog::logger> mp_physics;
		std::shared_ptr<spdlog::logger> mp_scene;
		std::shared_ptr<spdlog::logger> mp_resources;
	};

}
