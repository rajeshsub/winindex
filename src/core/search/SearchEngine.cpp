#include "SearchEngine.h"
#include "SimdSearch.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <thread>
#include <future>
#include <mutex>
#include <re2/re2.h>

namespace winindex {

// Convert wide string to UTF-8 for RE2
std::string SearchEngine::WideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string r(sz - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, r.data(), sz, nullptr, nullptr);
    return r;
}

// Very basic diacritic normalization via NFC -> ASCII fold using WinAPI
std::wstring SearchEngine::NormalizeDiacritics(const std::wstring& s) {
    // FoldString with MAP_PRECOMPOSED + custom: use LCMapString for normalization
    int needed = LCMapStringEx(LOCALE_NAME_INVARIANT,
                                LCMAP_LINGUISTIC_CASING | LCMAP_LOWERCASE,
                                s.c_str(), static_cast<int>(s.size()),
                                nullptr, 0, nullptr, nullptr, 0);
    if (needed <= 0) return s;
    std::wstring result(needed, L'\0');
    LCMapStringEx(LOCALE_NAME_INVARIANT,
                   LCMAP_LINGUISTIC_CASING | LCMAP_LOWERCASE,
                   s.c_str(), static_cast<int>(s.size()),
                   result.data(), needed, nullptr, nullptr, 0);
    return result;
}

bool SearchEngine::MatchesWholeWord(const std::wstring& text, size_t pos, size_t len) {
    auto isWordChar = [](wchar_t c) { return iswalnum(c) || c == L'_'; };
    if (pos > 0 && isWordChar(text[pos - 1])) return false;
    if (pos + len < text.size() && isWordChar(text[pos + len])) return false;
    return true;
}

std::vector<SearchResult> SearchEngine::Search(
    const std::wstring& query,
    const FileEntry*    entries,
    uint64_t            entryCount,
    const SearchOptions& options,
    uint32_t            maxResults,
    const std::atomic<bool>& cancelToken) {

    if (query.size() < 2) return {};

    if (options.useRegex)
        return SearchRegex(query, entries, entryCount, options, maxResults, cancelToken);
    else
        return SearchSubstring(query, entries, entryCount, options, maxResults, cancelToken);
}

std::vector<SearchResult> SearchEngine::SearchRegex(
    const std::wstring& query,
    const FileEntry*    entries,
    uint64_t            entryCount,
    const SearchOptions& options,
    uint32_t            maxResults,
    const std::atomic<bool>& cancelToken) {

    RE2::Options re2opts;
    re2opts.set_case_sensitive(options.caseSensitive);
    re2opts.set_encoding(RE2::Options::EncodingUTF8);

    std::string utf8Query = WideToUtf8(query);
    RE2 re(utf8Query, re2opts);
    if (!re.ok()) return {}; // Invalid regex — return empty

    std::vector<SearchResult> results;
    results.reserve(maxResults);

    for (uint64_t i = 0; i < entryCount && !cancelToken.load(std::memory_order_relaxed); ++i) {
        const FileEntry& e = entries[i];
        const std::wstring& target = options.matchPath ? e.path : e.name;

        std::string utf8Target = WideToUtf8(target);
        re2::StringPiece match;
        if (!RE2::PartialMatch(utf8Target, re, &match)) continue;

        if (options.wholeWord) {
            // Convert match offset back to wchar position (approximate for ASCII-range)
            size_t matchPos = static_cast<size_t>(match.data() - utf8Target.data());
            if (!MatchesWholeWord(target, matchPos, match.size())) continue;
        }

        SearchResult sr;
        sr.entry      = &e;
        sr.matchStart = 0;
        sr.matchLen   = 0;
        results.push_back(sr);

        if (results.size() >= maxResults) break;
    }
    return results;
}

std::vector<SearchResult> SearchEngine::SearchSubstring(
    const std::wstring& query,
    const FileEntry*    entries,
    uint64_t            entryCount,
    const SearchOptions& options,
    uint32_t            maxResults,
    const std::atomic<bool>& cancelToken) {

    std::wstring needle = options.caseSensitive ? query : [&]{
        std::wstring q = query;
        std::transform(q.begin(), q.end(), q.begin(), ::towlower);
        return q;
    }();

    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    uint64_t chunkSize = (entryCount + numThreads - 1) / numThreads;

    std::vector<std::future<std::vector<SearchResult>>> futures;
    futures.reserve(numThreads);

    for (unsigned int t = 0; t < numThreads; ++t) {
        uint64_t begin = t * chunkSize;
        uint64_t end   = std::min(begin + chunkSize, entryCount);
        if (begin >= entryCount) break;

        futures.push_back(std::async(std::launch::async,
            [&, begin, end]() -> std::vector<SearchResult> {
                std::vector<SearchResult> local;
                for (uint64_t i = begin; i < end; ++i) {
                    if (cancelToken.load(std::memory_order_relaxed)) break;

                    const FileEntry& e = entries[i];
                    const std::wstring& target = options.matchPath ? e.path : e.name;

                    std::wstring haystack = options.caseSensitive ? target : [&]{
                        std::wstring h = target;
                        std::transform(h.begin(), h.end(), h.begin(), ::towlower);
                        return h;
                    }();

                    size_t pos = SimdFindSubstring(
                        haystack.c_str(), haystack.size(),
                        needle.c_str(),   needle.size());

                    if (pos == std::wstring::npos) continue;
                    if (options.wholeWord && !MatchesWholeWord(haystack, pos, needle.size()))
                        continue;

                    SearchResult sr;
                    sr.entry      = &e;
                    sr.matchStart = static_cast<uint32_t>(pos);
                    sr.matchLen   = static_cast<uint32_t>(needle.size());
                    local.push_back(sr);
                }
                return local;
            }));
    }

    std::vector<SearchResult> results;
    results.reserve(maxResults);

    for (auto& f : futures) {
        auto chunk = f.get();
        for (auto& sr : chunk) {
            if (results.size() >= maxResults) goto done;
            results.push_back(sr);
        }
    }
done:
    return results;
}

} // namespace winindex
