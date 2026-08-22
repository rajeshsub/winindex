#pragma once
#include <mutex>
#include <string>

namespace winindex {

// Highest to lowest severity, per project logging convention (rule 17) - not RFC 5424.
enum class LogLevel { Critical = 0, Error = 1, Warning = 2, Info = 3, Debug = 4, Verbose = 5 };

std::wstring ToWString(LogLevel level);

// Case-insensitive match against a LogLevel name (e.g. "WARNING"). False and outLevel
// unchanged if text doesn't match a known level.
bool TryParseLogLevel(const std::wstring& text, LogLevel& outLevel);

class Logger {
public:
    static Logger& Instance();

    void Init(const std::wstring& logPath, LogLevel level = LogLevel::Warning);

    // Runtime-configurable verbosity: no re-Init needed to change the effective threshold.
    void SetLevel(LogLevel level);

    // Messages more verbose than the current threshold are silently dropped.
    void Log(LogLevel level, const std::wstring& message);

private:
    Logger() = default;
    std::wstring m_path;
    std::mutex m_mutex;
    LogLevel m_level = LogLevel::Warning;
};

}  // namespace winindex
