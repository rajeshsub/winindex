#include <gtest/gtest.h>

#include "settings/Settings.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace winindex;

class SettingsTest : public ::testing::Test {
protected:
    std::wstring tmpDir;
    std::unique_ptr<Settings> settings;

    void SetUp() override {
        wchar_t tmp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tmp);
        tmpDir = std::wstring(tmp) + L"winindex_settings_test";
        CreateDirectoryW(tmpDir.c_str(), nullptr);
        settings = std::make_unique<Settings>(true /*portable*/, tmpDir);
    }

    void TearDown() override {
        std::wstring iniPath = tmpDir + L"\\winindex.ini";
        _wremove(iniPath.c_str());
        RemoveDirectoryW(tmpDir.c_str());
    }
};

TEST_F(SettingsTest, DefaultReindexInterval) {
    settings->Load();
    EXPECT_EQ(settings->GetReindexIntervalHours(), kReindexDefaultHours);
}

TEST_F(SettingsTest, PersistReindexInterval) {
    settings->Load();
    settings->SetReindexIntervalHours(72);
    settings->Save();

    Settings s2(true, tmpDir);
    s2.Load();
    EXPECT_EQ(s2.GetReindexIntervalHours(), 72u);
}

TEST_F(SettingsTest, ManualOnlyPersists) {
    settings->Load();
    settings->SetReindexIntervalHours(kReindexManualOnly);
    settings->Save();

    Settings s2(true, tmpDir);
    s2.Load();
    EXPECT_EQ(s2.GetReindexIntervalHours(), kReindexManualOnly);
}

TEST_F(SettingsTest, SearchOptionsPersist) {
    settings->Load();
    SearchOptions opts{};
    opts.useRegex = true;
    opts.caseSensitive = true;
    opts.wholeWord = false;
    opts.matchPath = true;
    opts.ignoreDiacritics = false;
    settings->SetSearchOptions(opts);
    settings->Save();

    Settings s2(true, tmpDir);
    s2.Load();
    auto loaded = s2.GetSearchOptions();
    EXPECT_TRUE(loaded.useRegex);
    EXPECT_TRUE(loaded.caseSensitive);
    EXPECT_FALSE(loaded.wholeWord);
    EXPECT_TRUE(loaded.matchPath);
    EXPECT_FALSE(loaded.ignoreDiacritics);
}

TEST_F(SettingsTest, SelectedDrivesPersist) {
    settings->Load();
    settings->SetSelectedDrives({L"C:\\", L"D:\\"});
    settings->Save();

    Settings s2(true, tmpDir);
    s2.Load();
    auto drives = s2.GetSelectedDrives();
    ASSERT_EQ(drives.size(), 2u);
    EXPECT_EQ(drives[0], L"C:\\");
    EXPECT_EQ(drives[1], L"D:\\");
}

TEST_F(SettingsTest, ExcludedPathsPersist) {
    settings->Load();
    settings->SetExcludedPaths({L"C:\\Windows", L"C:\\Program Files"});
    settings->Save();

    Settings s2(true, tmpDir);
    s2.Load();
    auto excls = s2.GetExcludedPaths();
    ASSERT_EQ(excls.size(), 2u);
    EXPECT_EQ(excls[0], L"C:\\Windows");
    EXPECT_EQ(excls[1], L"C:\\Program Files");
}

TEST_F(SettingsTest, FirstRunFlag) {
    settings->Load();
    EXPECT_TRUE(settings->IsFirstRun());
    settings->SetFirstRunComplete();
    settings->Save();

    Settings s2(true, tmpDir);
    s2.Load();
    EXPECT_FALSE(s2.IsFirstRun());
}

TEST_F(SettingsTest, DataDirectoryIsExeDir) {
    EXPECT_EQ(settings->GetDataDirectory(), tmpDir);
}

TEST_F(SettingsTest, DefaultLogLevelIsWarning) {
    settings->Load();
    EXPECT_EQ(settings->GetLogLevel(), LogLevel::Warning);
}

TEST_F(SettingsTest, LogLevelPersists) {
    settings->Load();
    settings->SetLogLevel(LogLevel::Debug);
    settings->Save();

    Settings s2(true, tmpDir);
    s2.Load();
    EXPECT_EQ(s2.GetLogLevel(), LogLevel::Debug);
}

TEST_F(SettingsTest, GarbageLogLevelFallsBackToWarning) {
    settings->Load();
    settings->Save();
    // Corrupt the ini directly with an unrecognized value.
    std::wstring iniPath = tmpDir + L"\\winindex.ini";
    WritePrivateProfileStringW(L"General", L"LogLevel", L"NOT_A_LEVEL", iniPath.c_str());

    Settings s2(true, tmpDir);
    s2.Load();
    EXPECT_EQ(s2.GetLogLevel(), LogLevel::Warning);
}
