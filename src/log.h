#pragma once

#include <spdlog/spdlog.h>
#include <utility>

namespace VoxisLog
{
template <typename... Args> void critical(fmt::format_string<Args...> fmt, Args&&... args)
{
    spdlog::critical(fmt, std::forward<Args>(args)...);
    throw std::runtime_error("");
}

template <typename... Args> void error(fmt::format_string<Args...> fmt, Args&&... args)
{
    spdlog::error(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void debug(fmt::format_string<Args...> fmt, Args&&... args)
{
    spdlog::debug(fmt, std::forward<Args>(args)...);
}

template <typename... Args> void info(fmt::format_string<Args...> fmt, Args&&... args)
{
    spdlog::info(fmt, std::forward<Args>(args)...);
}
}; // namespace VoxisLog
