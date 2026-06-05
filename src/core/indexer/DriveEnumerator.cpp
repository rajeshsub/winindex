#include "DriveEnumerator.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

namespace winindex {

DriveFilesystem GetFilesystem(const std::wstring& root) {
    wchar_t fsName[MAX_PATH + 1] = {};
    if (!GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fsName,
                               static_cast<DWORD>(std::size(fsName)))) {
        return DriveFilesystem::Other;
    }
    if (wcscmp(fsName, L"NTFS") == 0)
        return DriveFilesystem::NTFS;
    if (wcscmp(fsName, L"FAT32") == 0 || wcscmp(fsName, L"FAT") == 0)
        return DriveFilesystem::FAT32;
    return DriveFilesystem::Other;
}

std::vector<DriveInfo> EnumerateLocalFixedDrives() {
    std::vector<DriveInfo> result;

    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i)))
            continue;

        wchar_t root[4] = {static_cast<wchar_t>(L'A' + i), L':', L'\\', L'\0'};
        if (GetDriveTypeW(root) != DRIVE_FIXED)
            continue;

        DriveInfo info;
        info.root = root;
        info.filesystem = GetFilesystem(root);

        // Skip filesystems we can't index
        if (info.filesystem == DriveFilesystem::Other)
            continue;

        wchar_t label[MAX_PATH + 1] = {};
        GetVolumeInformationW(root, label, static_cast<DWORD>(std::size(label)), nullptr, nullptr,
                              nullptr, nullptr, 0);
        info.label = label;

        ULARGE_INTEGER totalBytes{}, freeBytes{};
        GetDiskFreeSpaceExW(root, nullptr, &totalBytes, &freeBytes);
        info.totalBytes = totalBytes.QuadPart;
        info.freeBytes = freeBytes.QuadPart;

        result.push_back(std::move(info));
    }
    return result;
}

}  // namespace winindex
