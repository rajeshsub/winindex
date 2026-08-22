#include <gtest/gtest.h>

#include "settings/Logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <fstream>
#include <sstream>

using namespace winindex;

namespace {

// Reads the log file as raw bytes rather than through a wide-char stream: the Logger writes
// UTF-8, and std::wifstream decodes via the system code page, not UTF-8, which would silently
// misdecode. Narrow byte comparison is exact for the ASCII-only content these tests write.
std::string ReadFileContents(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

class LoggerTest : public ::testing::Test {
protected:
    std::wstring logPath;

    void SetUp() override {
        wchar_t tmp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tmp);
        logPath = std::wstring(tmp) + L"winindex_logger_test.log";
        _wremove(logPath.c_str());
    }

    void TearDown() override { _wremove(logPath.c_str()); }
};

TEST_F(LoggerTest, MessageAtOrAboveThresholdIsWritten) {
    Logger::Instance().Init(logPath, LogLevel::Warning);
    Logger::Instance().Log(LogLevel::Error, L"disk read failed");

    auto contents = ReadFileContents(logPath);
    EXPECT_NE(contents.find("disk read failed"), std::string::npos);
}

TEST_F(LoggerTest, MessageBelowThresholdIsSuppressed) {
    Logger::Instance().Init(logPath, LogLevel::Warning);
    Logger::Instance().Log(LogLevel::Info, L"routine status update");

    auto contents = ReadFileContents(logPath);
    EXPECT_EQ(contents.find("routine status update"), std::string::npos);
}

TEST_F(LoggerTest, WrittenLineIsStructured) {
    Logger::Instance().Init(logPath, LogLevel::Info);
    Logger::Instance().Log(LogLevel::Info, L"application started");

    auto contents = ReadFileContents(logPath);
    EXPECT_NE(contents.find("level=INFO"), std::string::npos);
    EXPECT_NE(contents.find("msg=\"application started\""), std::string::npos);
}

TEST_F(LoggerTest, SetLevelChangesFilteringImmediately) {
    Logger::Instance().Init(logPath, LogLevel::Warning);
    Logger::Instance().Log(LogLevel::Debug, L"before raising verbosity");
    Logger::Instance().SetLevel(LogLevel::Debug);
    Logger::Instance().Log(LogLevel::Debug, L"after raising verbosity");

    auto contents = ReadFileContents(logPath);
    EXPECT_EQ(contents.find("before raising verbosity"), std::string::npos);
    EXPECT_NE(contents.find("after raising verbosity"), std::string::npos);
}
