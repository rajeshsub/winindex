#pragma once
#include "../indexer/IFileSystemScanner.h"
#include "../storage/IndexPool.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace winindex {

/// @brief Options controlling how a search query is matched against the index.
struct SearchOptions {
    bool useRegex = false;          ///< Interpret query as an RE2 regular expression.
    bool caseSensitive = false;     ///< Perform a case-sensitive comparison.
    bool wholeWord = false;         ///< Match only whole words (word-boundary check).
    bool matchPath = false;         ///< Match against the full path instead of filename only.
    bool ignoreDiacritics = false;  ///< Fold diacritical marks before comparing.
};

/// @brief A single match returned by a search query.
struct SearchResult {
    uint32_t entryIndex;  ///< Index into the IndexPool that produced this result.
    uint32_t matchStart;  ///< Character offset within the matched string (for highlight rendering).
    uint32_t
        matchLen;  ///< Length of the matched substring in characters (0 for regex/token matches).
};

/// @brief Interface for searching the in-memory file index.
class ISearchEngine {
public:
    virtual ~ISearchEngine() = default;

    /// @brief Searches the pool for files matching @p query.
    ///
    /// Callers must hold a shared read lock on IndexPool for the duration of
    /// this call (IndexStore::GetSearchMutex()).
    ///
    /// @param query       Search string or regular expression.
    /// @param meta        Pointer to the flat EntryMeta array.
    /// @param entryCount  Number of entries in the array.
    /// @param nameLowerPool  Flat pool of lowercased filenames (searched by default).
    /// @param pathPool       Flat pool of full paths (searched when matchPath is true).
    /// @param options     Match mode flags.
    /// @param maxResults  Maximum number of results to return.
    /// @param cancelToken Set to true externally to abort a long-running search.
    /// @return Matched entry indices with highlight offsets, capped at @p maxResults.
    virtual std::vector<SearchResult> Search(const std::wstring& query, const EntryMeta* meta,
                                             uint64_t entryCount, const wchar_t* nameLowerPool,
                                             const wchar_t* pathPool, const SearchOptions& options,
                                             uint32_t maxResults,
                                             const std::atomic<bool>& cancelToken) = 0;
};

}  // namespace winindex
