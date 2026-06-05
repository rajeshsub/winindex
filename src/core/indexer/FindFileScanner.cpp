#include "FindFileScanner.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <queue>

namespace winindex {

uint64_t FindFileScanner::FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

bool FindFileScanner::IsExcluded(const std::wstring& path,
                                 const std::vector<std::wstring>& excludedPaths) {
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    for (const auto& excl : excludedPaths) {
        std::wstring exclLower = excl;
        std::transform(exclLower.begin(), exclLower.end(), exclLower.begin(), ::towlower);
        // Require word-boundary: exact match OR the path is a child of the excluded dir.
        // Without this, "C:\Program" would wrongly exclude "C:\ProgramData".
        if (lower == exclLower)
            return true;
        if (exclLower.back() != L'\\')
            exclLower += L'\\';
        if (lower.starts_with(exclLower))
            return true;
    }
    return false;
}

void FindFileScanner::Scan(const ScanOptions& options, ScanCallback onFile,
                           ProgressCallback onProgress, const std::atomic<bool>& cancelToken) {
    uint64_t filesFound = 0;
    uint64_t skippedCount = 0;

    for (const auto& root : options.rootPaths) {
        if (cancelToken.load(std::memory_order_relaxed))
            break;
        ScanDirectory(root, options, onFile, onProgress, filesFound, skippedCount, cancelToken);
    }
}

void FindFileScanner::ScanDirectory(const std::wstring& dir, const ScanOptions& options,
                                    ScanCallback& onFile, ProgressCallback& onProgress,
                                    uint64_t& filesFound, uint64_t& skippedCount,
                                    const std::atomic<bool>& cancelToken) {
    // Iterative BFS to avoid stack overflow on deep trees
    std::queue<std::wstring> dirQueue;
    dirQueue.push(dir);

    while (!dirQueue.empty() && !cancelToken.load(std::memory_order_relaxed)) {
        std::wstring current = std::move(dirQueue.front());
        dirQueue.pop();

        if (IsExcluded(current, options.excludedPaths)) {
            ++skippedCount;
            continue;
        }

        std::wstring pattern = current;
        if (pattern.back() != L'\\')
            pattern += L'\\';
        pattern += L"*";

        WIN32_FIND_DATAW fd{};
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED)
                ++skippedCount;
            continue;
        }

        do {
            if (cancelToken.load(std::memory_order_relaxed))
                break;

            // Skip . and ..
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                continue;
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;

            std::wstring fullPath = current;
            if (fullPath.back() != L'\\')
                fullPath += L'\\';
            fullPath += fd.cFileName;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                dirQueue.push(fullPath);
            } else {
                FileEntry fe;
                fe.name = fd.cFileName;
                fe.nameLower = fe.name;
                std::transform(fe.nameLower.begin(), fe.nameLower.end(), fe.nameLower.begin(),
                               ::towlower);
                fe.path = fullPath;
                fe.size = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                fe.lastModified = FileTimeToUint64(fd.ftLastWriteTime);
                fe.attributes = fd.dwFileAttributes;
                onFile(fe);

                ++filesFound;
                if (filesFound % 10000 == 0 && onProgress) {
                    onProgress(filesFound, current);
                }
            }
        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);
    }
}

}  // namespace winindex
