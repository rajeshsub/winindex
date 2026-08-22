#include "Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <fstream>

namespace winindex {

std::wstring ToWString(LogLevel level) {
    switch (level) {
        case LogLevel::Critical:
            return L"CRITICAL";
        case LogLevel::Error:
            return L"ERROR";
        case LogLevel::Warning:
            return L"WARNING";
        case LogLevel::Info:
            return L"INFO";
        case LogLevel::Debug:
            return L"DEBUG";
        case LogLevel::Verbose:
            return L"VERBOSE";
    }
    return L"WARNING";
}

bool TryParseLogLevel(const std::wstring& text, LogLevel& outLevel) {
    std::wstring upper;
    upper.reserve(text.size());
    for (wchar_t c : text) upper += static_cast<wchar_t>(towupper(c));

    if (upper == L"CRITICAL") {
        outLevel = LogLevel::Critical;
    } else if (upper == L"ERROR") {
        outLevel = LogLevel::Error;
    } else if (upper == L"WARNING") {
        outLevel = LogLevel::Warning;
    } else if (upper == L"INFO") {
        outLevel = LogLevel::Info;
    } else if (upper == L"DEBUG") {
        outLevel = LogLevel::Debug;
    } else if (upper == L"VERBOSE") {
        outLevel = LogLevel::Verbose;
    } else {
        return false;
    }
    return true;
}

Logger& Logger::Instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::Init(const std::wstring& logPath, LogLevel level) {
    std::lock_guard lock(m_mutex);
    m_path = logPath;
    m_level = level;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[96];
    snprintf(buf, sizeof(buf), "\n--- Session started %04d-%02d-%02d %02d:%02d:%02d ---\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::ofstream f(m_path, std::ios::app);
    if (f)
        f.write(buf, static_cast<std::streamsize>(strlen(buf)));
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard lock(m_mutex);
    m_level = level;
}

void Logger::Log(LogLevel level, const std::wstring& message) {
    std::lock_guard lock(m_mutex);
    if (m_path.empty() || level > m_level)
        return;

    // Structured key=value line: timestamp, level, and a quoted/escaped msg field.
    std::wstring escaped;
    escaped.reserve(message.size());
    for (wchar_t c : message) {
        if (c == L'"' || c == L'\\')
            escaped += L'\\';
        escaped += c;
    }
    std::wstring line = L"level=" + ToWString(level) + L" msg=\"" + escaped + L"\"";

    SYSTEMTIME st{};
    GetLocalTime(&st);

    int sz = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8;
    if (sz > 1) {
        utf8.resize(sz - 1);
        WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, utf8.data(), sz, nullptr, nullptr);
    }

    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d  ", st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::ofstream f(m_path, std::ios::app);
    if (!f)
        return;
    f.write(timeBuf, static_cast<std::streamsize>(strlen(timeBuf)));
    f.write(utf8.c_str(), static_cast<std::streamsize>(utf8.size()));
    f.write("\n", 1);
}

}  // namespace winindex
