#include "MftScanner.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <winioctl.h>

#include <algorithm>
#include <string>
#include <unordered_map>

namespace winindex {

bool MftScanner::IsMftAvailable(const std::wstring& root) const {
    // Attempt to open the volume with the access needed for FSCTL_ENUM_USN_DATA
    std::wstring volPath = L"\\\\.\\" + root.substr(0, 2);  // e.g. "\\.\C:"
    HANDLE hVol = CreateFileW(volPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (hVol == INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(hVol);
    return true;
}

bool MftScanner::IsExcluded(const std::wstring& path,
                            const std::vector<std::wstring>& excludedPaths) {
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& excl : excludedPaths) {
        std::wstring exclLower = excl;
        std::transform(exclLower.begin(), exclLower.end(), exclLower.begin(), ::towlower);
        if (lower == exclLower)
            return true;
        if (exclLower.back() != L'\\')
            exclLower += L'\\';
        if (lower.starts_with(exclLower))
            return true;
    }
    return false;
}

void MftScanner::Scan(const ScanOptions& options, ScanCallback onFile, ProgressCallback onProgress,
                      const std::atomic<bool>& cancelToken) {
    for (const auto& root : options.rootPaths) {
        if (cancelToken.load(std::memory_order_relaxed))
            return;
        ScanVolume(root, options, onFile, onProgress, cancelToken);
    }
}

bool MftScanner::ScanVolume(const std::wstring& root, const ScanOptions& options,
                            ScanCallback& onFile, ProgressCallback& onProgress,
                            const std::atomic<bool>& cancelToken) {
    std::wstring volPath = L"\\\\.\\" + root.substr(0, 2);
    HANDLE hVol = CreateFileW(volPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    if (hVol == INVALID_HANDLE_VALUE)
        return false;

    // Build FRN -> (name, parentFRN) map from MFT
    struct MftEntry {
        std::wstring name;
        DWORDLONG parentFrn;
        uint64_t size;
        uint64_t lastModified;
        uint32_t attributes;
        bool isDirectory;
    };

    std::unordered_map<DWORDLONG, MftEntry> entries;

    MFT_ENUM_DATA_V0 med{};
    med.StartFileReferenceNumber = 0;
    med.LowUsn = 0;
    med.HighUsn = MAXLONGLONG;

    constexpr DWORD bufSize = 1024 * 1024;  // 1 MB buffer
    std::vector<BYTE> buf(bufSize);
    DWORD bytesReturned = 0;
    uint64_t filesFound = 0;

    while (!cancelToken.load(std::memory_order_relaxed)) {
        if (!DeviceIoControl(hVol, FSCTL_ENUM_USN_DATA, &med, sizeof(med), buf.data(), bufSize,
                             &bytesReturned, nullptr)) {
            break;
        }

        USN_RECORD* record = reinterpret_cast<USN_RECORD*>(buf.data() + sizeof(USN));
        while (reinterpret_cast<BYTE*>(record) < buf.data() + bytesReturned) {
            if (record->FileNameLength > 0) {
                MftEntry entry;
                entry.name =
                    std::wstring(record->FileName, record->FileNameLength / sizeof(wchar_t));
                entry.parentFrn = record->ParentFileReferenceNumber;
                entry.size = 0;  // USN records don't carry file size
                // USN records don't carry timestamps; we'll resolve via
                // GetFileInformationByHandleEx
                entry.lastModified = 0;
                entry.attributes = record->FileAttributes;
                entry.isDirectory = (record->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

                entries[record->FileReferenceNumber] = std::move(entry);
                ++filesFound;

                if (filesFound % 10000 == 0 && onProgress) {
                    onProgress(filesFound, root);
                }
            }
            record = reinterpret_cast<USN_RECORD*>(reinterpret_cast<BYTE*>(record) +
                                                   record->RecordLength);
        }

        med.StartFileReferenceNumber = *reinterpret_cast<USN*>(buf.data());
    }

    CloseHandle(hVol);

    // Resolve full paths and emit non-directory entries
    std::function<std::wstring(DWORDLONG)> resolvePath = [&](DWORDLONG frn) -> std::wstring {
        auto it = entries.find(frn);
        if (it == entries.end())
            return root;
        if (it->second.parentFrn == frn)
            return root;  // root sentinel
        return resolvePath(it->second.parentFrn) + it->second.name + L"\\";
    };

    for (auto& [frn, entry] : entries) {
        if (cancelToken.load(std::memory_order_relaxed))
            break;
        if (entry.isDirectory)
            continue;

        std::wstring dir = resolvePath(entry.parentFrn);
        std::wstring full = dir + entry.name;

        if (IsExcluded(full, options.excludedPaths))
            continue;

        FileEntry fe;
        fe.name = entry.name;
        fe.nameLower = fe.name;
        std::transform(fe.nameLower.begin(), fe.nameLower.end(), fe.nameLower.begin(), ::towlower);
        fe.path = full;
        fe.size = entry.size;
        fe.lastModified = entry.lastModified;
        fe.attributes = entry.attributes;
        onFile(fe);
    }

    return true;
}

}  // namespace winindex
