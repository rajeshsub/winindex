#include "Settings.h"

#include "PathUtils.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shlobj.h>

#include <algorithm>
#include <sstream>

namespace winindex {

Settings::Settings(bool portableMode, const std::wstring& exeDir) : m_portable(portableMode) {
    if (portableMode) {
        m_dataDir = exeDir;
    } else {
        wchar_t appData[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData);
        m_dataDir = std::wstring(appData) + L"\\winindex";
        CreateDirectoryW(m_dataDir.c_str(), nullptr);
    }
    m_iniPath = m_dataDir + L"\\winindex.ini";
}

const std::wstring& Settings::GetDataDirectory() const {
    return m_dataDir;
}

void Settings::Load() {
    m_firstRun = (ReadInt(L"General", L"FirstRunComplete", 0) == 0);

    // Selected drives
    std::wstring drivesStr = ReadString(L"Indexing", L"SelectedDrives");
    m_selectedDrives.clear();
    if (!drivesStr.empty()) {
        std::wistringstream ss(drivesStr);
        std::wstring token;
        while (std::getline(ss, token, L';'))
            if (!token.empty())
                m_selectedDrives.push_back(token);
    }

    // Excluded paths: use saved list if present, otherwise seed with defaults.
    std::wstring exclStr = ReadString(L"Indexing", L"ExcludedPaths");
    if (exclStr.empty()) {
        m_excludedPaths = DefaultExcludedPaths();
    } else {
        m_excludedPaths.clear();
        std::wistringstream ss(exclStr);
        std::wstring token;
        while (std::getline(ss, token, L'|'))
            if (!token.empty())
                m_excludedPaths.push_back(token);
    }

    m_reindexIntervalHours = static_cast<uint64_t>(
        ReadInt(L"Indexing", L"ReindexIntervalHours", static_cast<int>(kReindexDefaultHours)));

    m_searchOptions.useRegex = ReadInt(L"Search", L"UseRegex", 0) != 0;
    m_searchOptions.caseSensitive = ReadInt(L"Search", L"CaseSensitive", 0) != 0;
    m_searchOptions.wholeWord = ReadInt(L"Search", L"WholeWord", 0) != 0;
    m_searchOptions.matchPath = ReadInt(L"Search", L"MatchPath", 0) != 0;
    m_searchOptions.ignoreDiacritics = ReadInt(L"Search", L"IgnoreDiacritics", 0) != 0;
}

void Settings::Save() const {
    WriteInt(L"General", L"FirstRunComplete", m_firstRun ? 0 : 1);

    std::wstring drivesStr;
    for (const auto& d : m_selectedDrives) drivesStr += d + L";";
    WriteString(L"Indexing", L"SelectedDrives", drivesStr);

    std::wstring exclStr;
    for (const auto& e : m_excludedPaths) exclStr += e + L"|";
    WriteString(L"Indexing", L"ExcludedPaths", exclStr);

    WriteInt(L"Indexing", L"ReindexIntervalHours", static_cast<int>(m_reindexIntervalHours));

    WriteInt(L"Search", L"UseRegex", m_searchOptions.useRegex ? 1 : 0);
    WriteInt(L"Search", L"CaseSensitive", m_searchOptions.caseSensitive ? 1 : 0);
    WriteInt(L"Search", L"WholeWord", m_searchOptions.wholeWord ? 1 : 0);
    WriteInt(L"Search", L"MatchPath", m_searchOptions.matchPath ? 1 : 0);
    WriteInt(L"Search", L"IgnoreDiacritics", m_searchOptions.ignoreDiacritics ? 1 : 0);
}

const std::vector<std::wstring>& Settings::GetSelectedDrives() const {
    return m_selectedDrives;
}
void Settings::SetSelectedDrives(std::vector<std::wstring> drives) {
    m_selectedDrives = std::move(drives);
}

const std::vector<std::wstring>& Settings::GetExcludedPaths() const {
    return m_excludedPaths;
}
void Settings::SetExcludedPaths(std::vector<std::wstring> paths) {
    m_excludedPaths = std::move(paths);
}

uint64_t Settings::GetReindexIntervalHours() const {
    return m_reindexIntervalHours;
}
void Settings::SetReindexIntervalHours(uint64_t h) {
    m_reindexIntervalHours = h;
}

SearchOptions Settings::GetSearchOptions() const {
    return m_searchOptions;
}
void Settings::SetSearchOptions(const SearchOptions& o) {
    m_searchOptions = o;
}

bool Settings::IsFirstRun() const {
    return m_firstRun;
}
void Settings::SetFirstRunComplete() {
    m_firstRun = false;
}

std::vector<std::wstring> Settings::DefaultExcludedPaths() {
    // Find the actual system drive so these exclusions work on D:, E:, etc.
    wchar_t winDir[MAX_PATH]{};
    GetWindowsDirectoryW(winDir, MAX_PATH);
    std::wstring sysDrive(winDir, 3);  // e.g. "C:\"

    auto p = [&](const wchar_t* rel) { return sysDrive + rel; };

    std::vector<std::wstring> paths = {
        p(L"Windows"),     p(L"Program Files"), p(L"Program Files (x86)"),
        p(L"ProgramData"), p(L"$Recycle.Bin"),  p(L"System Volume Information"),
        p(L"drivers"),
    };

    // Exclude per-user AppData folders (Roaming and Local) — large caches, temp files
    auto addShellPath = [&](int csidl) {
        wchar_t buf[MAX_PATH]{};
        if (SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK)
            paths.emplace_back(buf);
    };
    addShellPath(CSIDL_APPDATA);        // %APPDATA%  (Roaming)
    addShellPath(CSIDL_LOCAL_APPDATA);  // %LOCALAPPDATA%

    return paths;
}

void Settings::WriteString(const wchar_t* section, const wchar_t* key,
                           const std::wstring& value) const {
    WritePrivateProfileStringW(section, key, value.c_str(), m_iniPath.c_str());
}

std::wstring Settings::ReadString(const wchar_t* section, const wchar_t* key,
                                  const std::wstring& defaultVal) const {
    wchar_t buf[4096]{};
    GetPrivateProfileStringW(section, key, defaultVal.c_str(), buf,
                             static_cast<DWORD>(std::size(buf)), m_iniPath.c_str());
    return buf;
}

void Settings::WriteInt(const wchar_t* section, const wchar_t* key, int value) const {
    WritePrivateProfileStringW(section, key, std::to_wstring(value).c_str(), m_iniPath.c_str());
}

int Settings::ReadInt(const wchar_t* section, const wchar_t* key, int defaultVal) const {
    return static_cast<int>(GetPrivateProfileIntW(section, key, defaultVal, m_iniPath.c_str()));
}

}  // namespace winindex
