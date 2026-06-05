#pragma once
#include "IFileSystemScanner.h"
#include <atomic>

namespace winindex {

// Scans NTFS volumes via the Master File Table (requires admin privileges).
class MftScanner : public IFileSystemScanner {
public:
    bool IsMftAvailable(const std::wstring& root) const override;
    void Scan(const ScanOptions& options, ScanCallback onFile, ProgressCallback onProgress,
              const std::atomic<bool>& cancelToken) override;

private:
    bool ScanVolume(const std::wstring& root, const ScanOptions& options, ScanCallback& onFile,
                    ProgressCallback& onProgress, const std::atomic<bool>& cancelToken);

    static bool IsExcluded(const std::wstring& path,
                           const std::vector<std::wstring>& excludedPaths);
};

}  // namespace winindex
