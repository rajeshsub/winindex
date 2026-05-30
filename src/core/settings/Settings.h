#pragma once
#include "../search/ISearchEngine.h"
#include <string>
#include <vector>
#include <cstdint>

namespace winindex {

// 0 = Manual only (never auto-reindex)
constexpr uint64_t kReindexManualOnly = 0;
constexpr uint64_t kReindexDefaultHours = 48;

class Settings {
public:
    explicit Settings(bool portableMode, const std::wstring& exeDir);

    void Load();
    void Save() const;

    // Data directory (where index + log + INI live)
    std::wstring GetDataDirectory() const;

    // Drives
    std::vector<std::wstring> GetSelectedDrives() const;
    void SetSelectedDrives(std::vector<std::wstring> drives);

    // Exclusions
    std::vector<std::wstring> GetExcludedPaths() const;
    void SetExcludedPaths(std::vector<std::wstring> paths);

    // Reindex interval (hours). 0 = manual only.
    uint64_t GetReindexIntervalHours() const;
    void     SetReindexIntervalHours(uint64_t hours);

    // Search options
    SearchOptions GetSearchOptions() const;
    void          SetSearchOptions(const SearchOptions& opts);

    bool IsFirstRun() const;
    void SetFirstRunComplete();

    bool IsPortableMode() const { return m_portable; }

private:
    bool         m_portable;
    std::wstring m_dataDir;
    std::wstring m_iniPath;

    std::vector<std::wstring> m_selectedDrives;
    std::vector<std::wstring> m_excludedPaths;
    uint64_t                  m_reindexIntervalHours = kReindexDefaultHours;
    SearchOptions             m_searchOptions{};
    bool                      m_firstRun = true;

    static std::vector<std::wstring> DefaultExcludedPaths();

    void WriteString(const wchar_t* section, const wchar_t* key, const std::wstring& value) const;
    std::wstring ReadString(const wchar_t* section, const wchar_t* key,
                             const std::wstring& defaultVal = L"") const;
    void WriteInt(const wchar_t* section, const wchar_t* key, int value) const;
    int  ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal = 0) const;
};

} // namespace winindex
