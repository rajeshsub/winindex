#pragma once
#include "../indexer/IFileSystemScanner.h"
#include <vector>
#include <string>
#include <cstdint>

namespace winindex {

struct SearchOptions {
    bool useRegex        = false;
    bool caseSensitive   = false;
    bool wholeWord       = false;
    bool matchPath       = false;
    bool ignoreDiacritics = false;
};

struct SearchResult {
    const FileEntry* entry;   // pointer into index — valid for lifetime of IIndexStore
    uint32_t         matchStart; // char offset in matched string (for highlight)
    uint32_t         matchLen;
};

class ISearchEngine {
public:
    virtual ~ISearchEngine() = default;

    virtual std::vector<SearchResult> Search(
        const std::wstring& query,
        const FileEntry*    entries,
        uint64_t            entryCount,
        const SearchOptions& options,
        uint32_t            maxResults,
        const std::atomic<bool>& cancelToken) = 0;
};

} // namespace winindex
