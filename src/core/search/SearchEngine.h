#pragma once
#include <re2/re2.h>

#include "ISearchEngine.h"
#include <memory>

namespace winindex {

class SearchEngine : public ISearchEngine {
public:
    std::vector<SearchResult> Search(const std::wstring& query, const EntryMeta* meta,
                                     uint64_t entryCount, const wchar_t* nameLowerPool,
                                     const wchar_t* pathPool, const SearchOptions& options,
                                     uint32_t maxResults,
                                     const std::atomic<bool>& cancelToken) override;

private:
    static std::vector<SearchResult> SearchRegex(const std::wstring& query, const EntryMeta* meta,
                                                 uint64_t entryCount, const wchar_t* nameLowerPool,
                                                 const wchar_t* pathPool,
                                                 const SearchOptions& options, uint32_t maxResults,
                                                 const std::atomic<bool>& cancelToken);

    static std::vector<SearchResult> SearchSubstring(
        const std::wstring& query, const EntryMeta* meta, uint64_t entryCount,
        const wchar_t* nameLowerPool, const wchar_t* pathPool, const SearchOptions& options,
        uint32_t maxResults, const std::atomic<bool>& cancelToken);

    static bool MatchesWholeWord(const wchar_t* text, size_t textLen, size_t matchPos,
                                 size_t matchLen);
    static std::string WideToUtf8(const wchar_t* s, size_t len);
    static void WideToUtf8(const wchar_t* s, size_t len, std::string& out);
};

}  // namespace winindex
