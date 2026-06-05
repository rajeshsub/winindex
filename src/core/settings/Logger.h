#pragma once
#include <mutex>
#include <string>

namespace winindex {

class Logger {
public:
    static Logger& Instance();

    void Init(const std::wstring& logPath);
    void Log(const std::wstring& message);

private:
    Logger() = default;
    std::wstring m_path;
    std::mutex   m_mutex;
};

} // namespace winindex
