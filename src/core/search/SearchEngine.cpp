#include "SearchEngine.h"

#include "SimdSearch.h"
#include "TokenMatcher.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <re2/re2.h>

#include <algorithm>
#include <future>
#include <mutex>
#include <thread>

namespace winindex {

// Convert wide string to UTF-8 for RE2
std::string SearchEngine::WideToUtf8(const std::wstring& s) {
    if (s.empty())
        return {};
    int sz = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string r(sz - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, r.data(), sz, nullptr, nullptr);
    return r;
}

bool SearchEngine::MatchesWholeWord(const std::wstring& text, size_t pos, size_t len) {
    auto isWordChar = [](wchar_t c) { return iswalnum(c) || c == L'_'; };
    if (pos > 0 && isWordChar(text[pos - 1]))
        return false;
    if (pos + len < text.size() && isWordChar(text[pos + len]))
        return false;
    return true;
}

std::vector<SearchResult> SearchEngine::Search(const std::wstring& query, const FileEntry* entries,
                                               uint64_t entryCount, const SearchOptions& options,
                                               uint32_t maxResults,
                                               const std::atomic<bool>& cancelToken) {
    if (query.size() < 2)
        return {};

    if (options.useRegex)
        return SearchRegex(query, entries, entryCount, options, maxResults, cancelToken);
    else
        return SearchSubstring(query, entries, entryCount, options, maxResults, cancelToken);
}

std::vector<SearchResult> SearchEngine::SearchRegex(const std::wstring& query,
                                                    const FileEntry* entries, uint64_t entryCount,
                                                    const SearchOptions& options,
                                                    uint32_t maxResults,
                                                    const std::atomic<bool>& cancelToken) {
    RE2::Options re2opts;
    re2opts.set_case_sensitive(options.caseSensitive);
    re2opts.set_encoding(RE2::Options::EncodingUTF8);

    std::string utf8Query = WideToUtf8(query);
    RE2 re(utf8Query, re2opts);
    if (!re.ok())
        return {};  // Invalid regex — return empty

    // Capture-group version used only for whole-word boundary checks.
    // RE2::PartialMatch with a StringPiece* arg returns false if the pattern
    // has no capture groups, so we keep the plain boolean call as the primary path.
    RE2 reCapture("(" + utf8Query + ")", re2opts);

    std::vector<SearchResult> results;
    results.reserve(maxResults);

    for (uint64_t i = 0; i < entryCount && !cancelToken.load(std::memory_order_relaxed); ++i) {
        const FileEntry& e = entries[i];
        const std::wstring& target = options.matchPath ? e.path : e.name;

        std::string utf8Target = WideToUtf8(target);

        if (options.wholeWord) {
            re2::StringPiece match;
            if (!reCapture.ok() || !RE2::PartialMatch(utf8Target, reCapture, &match))
                continue;
            size_t matchPos = static_cast<size_t>(match.data() - utf8Target.data());
            if (!MatchesWholeWord(target, matchPos, match.size()))
                continue;
        } else {
            if (!RE2::PartialMatch(utf8Target, re))
                continue;
        }

        SearchResult sr;
        sr.entry = &e;
        sr.matchStart = 0;
        sr.matchLen = 0;
        results.push_back(sr);

        if (results.size() >= maxResults)
            break;
    }
    return results;
}

std::vector<SearchResult> SearchEngine::SearchSubstring(
    const std::wstring& query, const FileEntry* entries, uint64_t entryCount,
    const SearchOptions& options, uint32_t maxResults, const std::atomic<bool>& cancelToken) {
    std::wstring needle = options.caseSensitive ? query : [&] {
        std::wstring q = query;
        std::transform(q.begin(), q.end(), q.begin(), ::towlower);
        return q;
    }();

    // Token-set matching: pre-compute once, shared read-only across threads.
    // Only activated when the query contains separator chars (space/_/-/.)
    // so single-word queries take the unmodified SIMD-only path.
    const bool doTokenMatch = !options.caseSensitive && TokenMatcher::QueryHasSeparators(query);
    // lowercaseQuery owns the storage that sortedQueryTokens views reference.
    std::wstring lowercaseQuery;
    std::vector<std::wstring_view> sortedQueryTokens;
    if (doTokenMatch) {
        lowercaseQuery = needle;  // needle is already lowercased at this point
        sortedQueryTokens = TokenMatcher::TokenizeView(lowercaseQuery);
        std::sort(sortedQueryTokens.begin(), sortedQueryTokens.end());
    }

    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    uint64_t chunkSize = (entryCount + numThreads - 1) / numThreads;

    std::vector<std::future<std::vector<SearchResult>>> futures;
    futures.reserve(numThreads);

    for (unsigned int t = 0; t < numThreads; ++t) {
        uint64_t begin = t * chunkSize;
        uint64_t end = std::min(begin + chunkSize, entryCount);
        if (begin >= entryCount)
            break;

        futures.push_back(
            std::async(std::launch::async, [&, begin, end]() -> std::vector<SearchResult> {
                // Thread-local buffer for matchPath case-insensitive (avoids per-entry alloc)
                thread_local std::wstring tlsPathBuf;

                std::vector<SearchResult> local;
                for (uint64_t i = begin; i < end; ++i) {
                    if (cancelToken.load(std::memory_order_relaxed))
                        break;

                    const FileEntry& e = entries[i];

                    // Hot path: name search uses pre-lowercased nameLower — zero allocation
                    const wchar_t* haystackData;
                    size_t haystackLen;
                    if (options.matchPath) {
                        if (options.caseSensitive) {
                            haystackData = e.path.c_str();
                            haystackLen = e.path.size();
                        } else {
                            tlsPathBuf.assign(e.path.begin(), e.path.end());
                            std::transform(tlsPathBuf.begin(), tlsPathBuf.end(), tlsPathBuf.begin(),
                                           ::towlower);
                            haystackData = tlsPathBuf.c_str();
                            haystackLen = tlsPathBuf.size();
                        }
                    } else {
                        const std::wstring& hay = options.caseSensitive ? e.name : e.nameLower;
                        haystackData = hay.c_str();
                        haystackLen = hay.size();
                    }

                    size_t pos =
                        SimdFindSubstring(haystackData, haystackLen, needle.c_str(), needle.size());

                    bool tokenMatch = false;
                    if (pos == std::wstring::npos) {
                        // Token-set fallback: fires only when SIMD missed and the query
                        // had separators (e.g. "just rosy", "rosy guitar flac").
                        if (!doTokenMatch)
                            continue;
                        std::wstring haystackStr(haystackData, haystackLen);
                        auto fnTokens = TokenMatcher::TokenizeView(haystackStr);
                        std::sort(fnTokens.begin(), fnTokens.end());
                        if (!TokenMatcher::AllQueryTokensPresent(sortedQueryTokens, fnTokens))
                            continue;
                        tokenMatch = true;
                        pos = 0;
                    }

                    if (options.wholeWord && !tokenMatch) {
                        std::wstring_view hayView(haystackData, haystackLen);
                        if (!MatchesWholeWord(std::wstring(hayView), pos, needle.size()))
                            continue;
                    }

                    SearchResult sr;
                    sr.entry = &e;
                    sr.matchStart = static_cast<uint32_t>(pos);
                    sr.matchLen = tokenMatch ? 0u : static_cast<uint32_t>(needle.size());
                    local.push_back(sr);
                }
                return local;
            }));
    }

    std::vector<SearchResult> results;
    results.reserve(maxResults);

    for (auto& f : futures) {
        auto chunk = f.get();
        for (const auto& sr : chunk) {
            if (results.size() >= maxResults)
                goto done;
            results.push_back(sr);
        }
    }
done:
    return results;
}

}  // namespace winindex
