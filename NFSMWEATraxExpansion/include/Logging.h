#pragma once

#include <filesystem>

namespace eatrax {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

void InitializeLogging(const std::filesystem::path& path);
void Log(LogLevel level, const char* format, ...);

}  // namespace eatrax
