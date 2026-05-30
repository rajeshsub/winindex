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
    if (CreateDirectoryW(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace winindex
