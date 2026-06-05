#pragma once
#include <string>

namespace winindex {

// Returns the directory containing the running executable.
std::wstring GetExeDirectory();

// Returns true if a file named winindex.ini exists next to the exe (portable mode).
bool IsPortableMode();

// Ensures directory exists, creates it if not.
bool EnsureDirectory(const std::wstring& path);

}  // namespace winindex
