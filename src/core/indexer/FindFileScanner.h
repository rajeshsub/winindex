#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "IFileSystemScanner.h"
#include <atomic>

namespace winindex {

// Scans any filesystem using FindFirstFile/FindNextFile.
// Used as fallback when MFT is unavailable, and as primary for FAT32.
class FindFileScanner : public IFileSystemScanner {
public:
    bool IsMftAvailable(const std::wstring& /*root*/) const override { return false; }

    void Scan(const ScanOptions& options,
              ScanCallback onFile,
              ProgressCallback onProgress,
              const std::atomic<bool>& cancelToken) override;

private:
    void ScanDirectory(const std::wstring& dir,
                       const ScanOptions& options,
                       ScanCallback& onFile,
                       ProgressCallback& onProgress,
                       uint64_t& filesFound,
                       uint64_t& skippedCount,
                       const std::atomic<bool>& cancelToken);

    static bool IsExcluded(const std::wstring& path,
                            const std::vector<std::wstring>& excludedPaths);

    static uint64_t FileTimeToUint64(const FILETIME& ft);
};

} // namespace winindex
