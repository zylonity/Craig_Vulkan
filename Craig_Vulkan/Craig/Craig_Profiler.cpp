#include "Craig_Profiler.hpp"
#include <cstdio>
#include <cstring>

void Craig::Profiler::addTime(const char* name, double ms) {

	for (Section& section : mv_sections) {
		if (std::strcmp(section.name, name) == 0) {
			section.totalMs += ms;
			return;
		}
	}

	mv_sections.push_back({ name, ms });
}

void Craig::Profiler::endFrame() {

	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	m_frameCount++;

	// only print once a second, otherwise it floods the console
	const double sinceLastPrintMs = std::chrono::duration<double, std::milli>(now - m_lastPrintTime).count();
	if (sinceLastPrintMs < 1000.0) {
		return;
	}

	printf("---- Profiler (avg ms per frame over %u frames, %.0f fps) ----\n", m_frameCount, m_frameCount * 1000.0 / sinceLastPrintMs);
	printf("  %-24s %.3f\n", "Frame (total)", sinceLastPrintMs / m_frameCount);
	for (Section& section : mv_sections) {
		printf("  %-24s %.3f\n", section.name, section.totalMs / m_frameCount);
		section.totalMs = 0.0;
	}

	m_frameCount = 0;
	m_lastPrintTime = now;
}

Craig::ScopedTimer::~ScopedTimer() {

	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_start).count();
	Profiler::getInstance().addTime(m_name, ms);
}
