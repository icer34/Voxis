#include "timer.h"

#include <chrono>
#include <mutex>
#include <unordered_map>

#include "log.h"

namespace
{
using Clock = std::chrono::steady_clock;

// per-thread: each thread tracks its own in-flight start() calls, so concurrent start()/stop()
// pairs under the same label from different threads never step on each other
thread_local std::unordered_map<std::string, Clock::time_point> _startTimes;

// global, aggregated across every thread - the only part that needs a lock
std::mutex _statsMutex;
std::unordered_map<std::string, Timer::Stats> _stats;
} // namespace

namespace Timer
{
void start(const std::string& label)
{
    _startTimes[label] = Clock::now();
}

double stop(const std::string& label)
{
    auto it = _startTimes.find(label);
    if (it == _startTimes.end())
    {
        VoxisLog::error("Timer::stop(\"{}\") called without a matching start() on this thread", label);
        return 0.0;
    }

    double elapsedSeconds = std::chrono::duration<double>(Clock::now() - it->second).count();
    _startTimes.erase(it);

    std::lock_guard<std::mutex> lock(_statsMutex);
    Stats& stats = _stats[label];
    stats.totalSeconds += elapsedSeconds;
    stats.callCount++;

    return elapsedSeconds;
}

Stats getStats(const std::string& label)
{
    std::lock_guard<std::mutex> lock(_statsMutex);
    auto it = _stats.find(label);
    return it != _stats.end() ? it->second : Stats{};
}

void reset(const std::string& label)
{
    std::lock_guard<std::mutex> lock(_statsMutex);
    _stats.erase(label);
}

void resetAll()
{
    std::lock_guard<std::mutex> lock(_statsMutex);
    _stats.clear();
}

void logAll()
{
    std::lock_guard<std::mutex> lock(_statsMutex);
    for (const auto& [label, stats] : _stats)
    {
        double avgMs = stats.callCount > 0 ? (stats.totalSeconds * 1000.0) / static_cast<double>(stats.callCount) : 0.0;
        VoxisLog::info("Timer[{}]: total={:.3f}s, calls={}, avg={:.3f}ms",
                       label,
                       stats.totalSeconds,
                       stats.callCount,
                       avgMs);
    }
}
} // namespace Timer
