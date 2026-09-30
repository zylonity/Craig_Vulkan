#include "Craig_Logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

CraigError Craig::Logger::init() {

	CraigError ret = CRAIG_SUCCESS;

	// Console gets colours, the level name is the coloured bit
	spdlog::sink_ptr consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	consoleSink->set_pattern("[%H:%M:%S.%e] [%n] [%^%l%$] %v");
	mv_sinks.push_back(consoleSink);

	// File gets the date too in case you're digging through an old one
	// true = start fresh every run instead of appending forever
	try {
		spdlog::sink_ptr fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(kLogFilePath, true);
		fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
		mv_sinks.push_back(fileSink);
	}
	catch (const spdlog::spdlog_ex& error) {
		// not worth crashing over, you just don't get a file
		spdlog::error("Couldn't open {}: {}", kLogFilePath, error.what());
		ret = CRAIG_FAIL;
	}

	// Editor window colours lines by level itself, so no %^ %$ here
	mp_editorSink = std::make_shared<EditorLogSink>();
	mp_editorSink->set_pattern("[%H:%M:%S] [%n] [%l] %v");
	mv_sinks.push_back(mp_editorSink);

	mp_engine = createLogger("Craig");
	mp_renderer = createLogger("Renderer");
	mp_vulkan = createLogger("Vulkan");
	mp_physics = createLogger("Physics");
	mp_scene = createLogger("Scene");
	mp_resources = createLogger("Resources");

	mp_engine->info("Logger's up, also writing to {}", kLogFilePath);

	return ret;
}

CraigError Craig::Logger::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	mp_engine->info("Shutting down, bye");

	// loggers stay alive, this just makes sure everything's on disk
	for (spdlog::sink_ptr& sink : mv_sinks) {
		sink->flush();
	}

	return ret;
}

spdlog::logger& Craig::Logger::get(const std::shared_ptr<spdlog::logger>& pLogger) {

	if (pLogger == nullptr) {
		return *spdlog::default_logger_raw();
	}

	return *pLogger;
}

std::shared_ptr<spdlog::logger> Craig::Logger::createLogger(const char* name) {

	std::shared_ptr<spdlog::logger> pLogger = std::make_shared<spdlog::logger>(name, mv_sinks.begin(), mv_sinks.end());

#if defined(_DEBUG)
	pLogger->set_level(spdlog::level::debug);
#else
	pLogger->set_level(spdlog::level::info);
#endif

	// Flush straight to disk for info and up, so a crash doesn't eat the lines right before it
	// fine as long as nothing logs info every frame
	// use debug/trace for that
	pLogger->flush_on(spdlog::level::info);

	return pLogger;
}

std::vector<Craig::LogEntry> Craig::EditorLogSink::getEntries() {

	std::lock_guard<std::mutex> lock(mutex_);
	return std::vector<LogEntry>(m_entries.begin(), m_entries.end());
}

uint64_t Craig::EditorLogSink::getVersion() {

	std::lock_guard<std::mutex> lock(mutex_);
	return m_version;
}

void Craig::EditorLogSink::clear() {

	std::lock_guard<std::mutex> lock(mutex_);
	m_entries.clear();
	m_version++;
}

// base_sink already holds mutex_ when it calls this, so no locking in here
void Craig::EditorLogSink::sink_it_(const spdlog::details::log_msg& msg) {

	spdlog::memory_buf_t formatted;
	formatter_->format(msg, formatted);

	// the formatter sticks a newline on the end, ImGui doesn't want it
	std::string text(formatted.data(), formatted.size());
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
		text.pop_back();
	}

	m_entries.push_back({ msg.level, std::move(text) });
	if (m_entries.size() > kMaxEditorLogLines) {
		m_entries.pop_front();
	}
	m_version++;
}
