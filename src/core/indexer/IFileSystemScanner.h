#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace winindex {

/// @brief Represents a single file entry in the index.
struct FileEntry {
    std::wstring name;  ///< Filename only (no directory components).
    std::wstring
        nameLower;  ///< Pre-computed lowercase name for zero-allocation case-insensitive search.
    std::wstring path;      ///< Full absolute path including filename.
    uint64_t size;          ///< File size in bytes.
    uint64_t lastModified;  ///< Last-write time as FILETIME (100-ns intervals since 1601-01-01).
    uint32_t attributes;    ///< Win32 file attributes (FILE_ATTRIBUTE_* flags).
};

/// @brief Callback invoked for each file discovered during a scan.
using ScanCallback = std::function<void(const FileEntry&)>;

/// @brief Callback invoked periodically during a scan to report progress.
using ProgressCallback = std::function<void(uint64_t filesFound, const std::wstring& currentDir)>;

/// @brief Options controlling which paths a scan covers.
struct ScanOptions {
    std::vector<std::wstring> rootPaths;      ///< Drive roots or directories to scan.
    std::vector<std::wstring> excludedPaths;  ///< Paths excluded recursively from the scan.
};

/// @brief Interface for scanning a file system and enumerating file entries.
///
/// Two implementations exist: MftScanner (NTFS MFT direct read, requires elevation)
/// and FindFileScanner (FindFirstFile BFS, works on all volumes).
class IFileSystemScanner {
public:
    virtual ~IFileSystemScanner() = default;

    /// @brief Returns true if an MFT/elevated scan is available for the given root.
    /// @param root Drive root path, e.g. L"C:\\".
    virtual bool IsMftAvailable(const std::wstring& root) const = 0;

    /// @brief Performs a full scan of the configured roots.
    /// @param options  Roots and exclusions to apply.
    /// @param onFile      Called for every file discovered.
    /// @param onProgress  Called periodically with files-found count and current directory.
    /// @param cancelToken Set to true externally to abort the scan early.
    virtual void Scan(const ScanOptions& options, ScanCallback onFile, ProgressCallback onProgress,
                      const std::atomic<bool>& cancelToken) = 0;
};

}  // namespace winindex
