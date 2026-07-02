#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace winindex {

// Returns the directory containing the running executable.
std::wstring GetExeDirectory();

// Returns true if a file named winindex.ini exists next to the exe (portable mode).
bool IsPortableMode();

// Ensures directory exists, creates it if not.
bool EnsureDirectory(const std::wstring& path);

// Formats a file count with thousands separators, e.g. 1234567 -> L"1,234,567".
std::wstring FormatFileCount(uint64_t n);

// Formats an age in seconds as a human-readable string:
//   < 60 s      -> L"just indexed"
//   < 3600 s    -> L"N min old"
//   < 172800 s  -> L"N hrs old"
//   >= 172800 s -> L"N days, N hrs old"
std::wstring FormatAge(uint64_t ageSeconds);

// Formats a list of drive/folder paths as a comma-separated string.
// Drive roots (e.g. L"C:\") are shortened to L"C:".
// Trailing backslashes on folder paths are stripped.
std::wstring FormatLocationList(const std::vector<std::wstring>& paths);

}  // namespace winindex
