#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace winindex {

struct FileEntry {
    std::wstring name;      // filename only
    std::wstring path;      // full path including filename
    uint64_t     size;
    uint64_t     lastModified; // FILETIME as uint64
    uint32_t     attributes;
};

// Called for each file discovered during scan.
using ScanCallback = std::function<void(const FileEntry&)>;

// Called periodically during scan to report progress.
using ProgressCallback = std::function<void(uint64_t filesFound, const std::wstring& currentDir)>;

struct ScanOptions {
    std::vector<std::wstring> rootPaths;
    std::vector<std::wstring> excludedPaths; // recursive exclusions
};

class IFileSystemScanner {
public:
    virtual ~IFileSystemScanner() = default;

    // Returns true if MFT/elevated scan is available for this root.
    virtual bool IsMftAvailable(const std::wstring& root) const = 0;

    // Full scan of root. Calls onFile for each file found.
    virtual void Scan(const ScanOptions& options,
                      ScanCallback onFile,
                      ProgressCallback onProgress,
                      const std::atomic<bool>& cancelToken) = 0;
};

} // namespace winindex
