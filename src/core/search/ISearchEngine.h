#pragma once
#include "../indexer/IFileSystemScanner.h"
#include <cstdint>
#include <string>
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
    const FileEntry*
        entry;            ///< Pointer into the live index — valid for the lifetime of IIndexStore.
    uint32_t matchStart;  ///< Character offset within the matched string (for highlight rendering).
    uint32_t matchLen;    ///< Length of the matched substring in characters.
};

/// @brief Interface for searching the in-memory file index.
///
/// Implementations: ScalarSearchEngine (simple substring), SimdSearchEngine
/// (AVX2/SSE4.2 accelerated), and RegexSearchEngine (RE2-based).
class ISearchEngine {
public:
    virtual ~ISearchEngine() = default;

    /// @brief Searches @p entries for files matching @p query.
    /// @param query       Search string or regular expression.
    /// @param entries     Pointer to the flat array of FileEntry objects in the index.
    /// @param entryCount  Number of entries in the array.
    /// @param options     Match mode flags.
    /// @param maxResults  Maximum number of results to return.
    /// @param cancelToken Set to true externally to abort a long-running search.
    /// @return Matched entries with highlight offsets, capped at @p maxResults.
    virtual std::vector<SearchResult> Search(const std::wstring& query, const FileEntry* entries,
                                             uint64_t entryCount, const SearchOptions& options,
                                             uint32_t maxResults,
                                             const std::atomic<bool>& cancelToken) = 0;
};

}  // namespace winindex
