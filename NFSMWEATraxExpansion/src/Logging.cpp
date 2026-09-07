#include "Logging.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace eatrax {
namespace {

std::filesystem::path g_logPath;
std::mutex g_logMutex;

const char* LevelName(const LogLevel level) {
    switch (level) {
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace

void InitializeLogging(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_logPath = path;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"w, ccs=UTF-8") == 0 && file != nullptr) {
        std::fwprintf(file, L"NFSMW EA TRAX Expansion log\n");
        std::fclose(file);
    }
}

void Log(const LogLevel level, const char* format, ...) {
    char message[2048]{};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    char line[2304]{};
    sprintf_s(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u.%03u [%s] %s\n",
              time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond,
              time.wMilliseconds, LevelName(level), message);
    OutputDebugStringA(line);

    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logPath.empty()) {
        return;
    }
    FILE* file = nullptr;
    if (_wfopen_s(&file, g_logPath.c_str(), L"a, ccs=UTF-8") != 0 || file == nullptr) {
        return;
    }
    int required = MultiByteToWideChar(CP_UTF8, 0, line, -1, nullptr, 0);
    if (required > 0) {
        std::wstring wide(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, line, -1, wide.data(), required);
        std::fputws(wide.c_str(), file);
    }
    std::fclose(file);
}

}  // namespace eatrax
