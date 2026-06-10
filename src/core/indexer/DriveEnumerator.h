#pragma once
#include <string>
#include <vector>

namespace winindex {

enum class DriveFilesystem { NTFS, FAT32, Other };

struct DriveInfo {
    std::wstring root;  // e.g. L"C:\\"
    std::wstring label;
    DriveFilesystem filesystem;
};

// Enumerates all local fixed drives (skips network, removable, CD-ROM).
std::vector<DriveInfo> EnumerateLocalFixedDrives();

// Enumerates fixed + removable drives (USB sticks, external HDDs); skips network and CD-ROM.
std::vector<DriveInfo> EnumerateAllDrives();

DriveFilesystem GetFilesystem(const std::wstring& root);

}  // namespace winindex
