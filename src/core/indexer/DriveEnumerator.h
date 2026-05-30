#pragma once
#include <string>
#include <vector>

namespace winindex {

enum class DriveFilesystem { NTFS, FAT32, Other };

struct DriveInfo {
    std::wstring    root;       // e.g. L"C:\\"
    std::wstring    label;
    DriveFilesystem filesystem;
    uint64_t        totalBytes;
    uint64_t        freeBytes;
};

// Enumerates all local fixed drives (skips network, removable, CD-ROM).
std::vector<DriveInfo> EnumerateLocalFixedDrives();

DriveFilesystem GetFilesystem(const std::wstring& root);

} // namespace winindex
