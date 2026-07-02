#include "PathUtils.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace winindex {

std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    size_t slash = p.rfind(L'\\');
    return (slash != std::wstring::npos) ? p.substr(0, slash) : p;
}

bool IsPortableMode() {
    std::wstring ini = GetExeDirectory() + L"\\winindex.ini";
    return GetFileAttributesW(ini.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool EnsureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr))
        return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

std::wstring FormatFileCount(uint64_t n) {
    std::wstring s = std::to_wstring(n);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), L",");
    return s;
}

std::wstring FormatAge(uint64_t ageSeconds) {
    if (ageSeconds < 60)
        return L"just indexed";
    if (ageSeconds < 3600)
        return std::to_wstring(ageSeconds / 60) + L" min old";
    if (ageSeconds < 172800)
        return std::to_wstring(ageSeconds / 3600) + L" hrs old";
    uint64_t days = ageSeconds / 86400;
    uint64_t hrs = (ageSeconds % 86400) / 3600;
    return std::to_wstring(days) + L" days, " + std::to_wstring(hrs) + L" hrs old";
}

std::wstring FormatLocationList(const std::vector<std::wstring>& paths) {
    std::wstring result;
    for (const auto& p : paths) {
        if (!result.empty())
            result += L", ";
        // Drive root: "C:\" -> "C:"
        if (p.size() == 3 && p[1] == L':' && p[2] == L'\\') {
            result += p.substr(0, 2);
        } else {
            // Strip trailing backslash from folder paths
            std::wstring trimmed = p;
            if (!trimmed.empty() && trimmed.back() == L'\\')
                trimmed.pop_back();
            result += trimmed;
        }
    }
    return result;
}

}  // namespace winindex
