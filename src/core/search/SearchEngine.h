#pragma once
#include "ISearchEngine.h"
#include <re2/re2.h>
#include <memory>

namespace winindex {

class SearchEngine : public ISearchEngine {
public:
    std::vector<SearchResult> Search(
        const std::wstring& query,
        const FileEntry*    entries,
        uint64_t            entryCount,
        const SearchOptions& options,
        uint32_t            maxResults,
        const std::atomic<bool>& cancelToken) override;

private:
    // Regex search path
    std::vector<SearchResult> SearchRegex(
        const std::wstring& query,
        const FileEntry*    entries,
        uint64_t            entryCount,
        const SearchOptions& options,
        uint32_t            maxResults,
        const std::atomic<bool>& cancelToken);

    // SIMD substring search path (parallelized)
    std::vector<SearchResult> SearchSubstring(
        const std::wstring& query,
        const FileEntry*    entries,
        uint64_t            entryCount,
        const SearchOptions& options,
        uint32_t            maxResults,
        const std::atomic<bool>& cancelToken);

    static bool MatchesWholeWord(const std::wstring& text, size_t matchPos, size_t matchLen);
    static std::wstring NormalizeDiacritics(const std::wstring& s);
    static std::string WideToUtf8(const std::wstring& s);
};

} // namespace winindex
