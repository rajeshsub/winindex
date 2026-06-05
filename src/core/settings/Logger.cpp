#include "Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>
#include <cstdio>

namespace winindex {

Logger& Logger::Instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::Init(const std::wstring& logPath) {
    std::lock_guard lock(m_mutex);
    m_path = logPath;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[96];
    snprintf(buf, sizeof(buf),
             "\n--- Session started %04d-%02d-%02d %02d:%02d:%02d ---\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::ofstream f(m_path, std::ios::app);
    if (f) f.write(buf, static_cast<std::streamsize>(strlen(buf)));
}

void Logger::Log(const std::wstring& message) {
    std::lock_guard lock(m_mutex);
    if (m_path.empty()) return;

    SYSTEMTIME st{};
    GetLocalTime(&st);

    int sz = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1,
                                  nullptr, 0, nullptr, nullptr);
    std::string utf8;
    if (sz > 1) {
        utf8.resize(sz - 1);
        WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1,
                             utf8.data(), sz, nullptr, nullptr);
    }

    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d  ",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::ofstream f(m_path, std::ios::app);
    if (!f) return;
    f.write(timeBuf, static_cast<std::streamsize>(strlen(timeBuf)));
    f.write(utf8.c_str(), static_cast<std::streamsize>(utf8.size()));
    f.write("\n", 1);
}

} // namespace winindex
