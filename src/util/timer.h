#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Simple named-section timer for ad-hoc profiling anywhere in the project - call start(label)
 * before a section of code and stop(label) after, from as many places/threads as you want. Safe to
 * call start/stop for the SAME label concurrently from multiple threads (each thread tracks its own
 * in-flight start times), the accumulated stats (total time, call count) are aggregated globally.
 *
 * Usage:
 *   Timer::start("terrainGen");
 *   ...
 *   Timer::stop("terrainGen"); // returns this call's elapsed seconds, adds it to the running total
 *
 *   Timer::logAll(); // dumps every recorded label's stats via VoxisLog::info
 */
namespace Timer
{
void start(const std::string& label);

/**
 * @brief Stops the most recent start() for this label on the CURRENT thread, adds the elapsed time
 * to the label's running total, and returns it (seconds). Logs an error and returns 0 if there was
 * no matching start() on this thread.
 */
double stop(const std::string& label);

struct Stats
{
    double totalSeconds = 0.0;
    uint32_t callCount = 0;
};

Stats getStats(const std::string& label);

void reset(const std::string& label);
void resetAll();

// logs every label recorded so far (total time, call count, average per call) via VoxisLog::info
void logAll();
} // namespace Timer
