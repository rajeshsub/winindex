#include "SearchEngine.h"

#include "SimdSearch.h"
#include "TokenMatcher.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <re2/re2.h>

#include <algorithm>
#include <atomic>
#include <future>
#include <thread>

namespace winindex {

std::string SearchEngine::WideToUtf8(const wchar_t* s, size_t len) {
    std::string r;
    WideToUtf8(s, len, r);
    return r;
}

void SearchEngine::WideToUtf8(const wchar_t* s, size_t len, std::string& out) {
    if (len == 0) {
        out.clear();
        return;
    }
    int sz =
        WideCharToMultiByte(CP_UTF8, 0, s, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    out.resize(static_cast<size_t>(sz));
    WideCharToMultiByte(CP_UTF8, 0, s, static_cast<int>(len), out.data(), sz, nullptr, nullptr);
}

bool SearchEngine::MatchesWholeWord(const wchar_t* text, size_t textLen, size_t pos, size_t len) {
    auto isWordChar = [](wchar_t c) { return iswalnum(c) || c == L'_'; };
    if (pos > 0 && isWordChar(text[pos - 1]))
        return false;
    if (pos + len < textLen && isWordChar(text[pos + len]))
        return false;
    return true;
}

std::vector<SearchResult> SearchEngine::Search(const std::wstring& query, const EntryMeta* meta,
                                               uint64_t entryCount, const wchar_t* nameLowerPool,
                                               const wchar_t* pathPool,
                                               const SearchOptions& options, uint32_t maxResults,
                                               const std::atomic<bool>& cancelToken) {
    if (query.size() < 2)
        return {};
    if (options.useRegex)
        return SearchRegex(query, meta, entryCount, nameLowerPool, pathPool, options, maxResults,
                           cancelToken);
    return SearchSubstring(query, meta, entryCount, nameLowerPool, pathPool, options, maxResults,
                           cancelToken);
}

std::vector<SearchResult> SearchEngine::SearchRegex(
    const std::wstring& query, const EntryMeta* meta, uint64_t entryCount,
    [[maybe_unused]] const wchar_t* nameLowerPool, const wchar_t* pathPool,
    const SearchOptions& options, uint32_t maxResults, const std::atomic<bool>& cancelToken) {
    RE2::Options re2opts;
    re2opts.set_case_sensitive(options.caseSensitive);
    re2opts.set_encoding(RE2::Options::EncodingUTF8);

    std::string utf8Query = WideToUtf8(query.c_str(), query.size());
    // Compile RE2 objects once; RE2::PartialMatch is thread-safe for concurrent reads.
    RE2 re(utf8Query, re2opts);
    if (!re.ok())
        return {};
    std::unique_ptr<RE2> reCapture;
    if (options.wholeWord)
        reCapture = std::make_unique<RE2>("(" + utf8Query + ")", re2opts);

    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    uint64_t chunkSize = (entryCount + numThreads - 1) / numThreads;
    std::atomic<uint32_t> collected{0};

    std::vector<std::future<std::vector<SearchResult>>> futures;
    futures.reserve(numThreads);

    for (unsigned int t = 0; t < numThreads; ++t) {
        uint64_t begin = t * chunkSize;
        uint64_t end = std::min(begin + chunkSize, entryCount);
        if (begin >= entryCount)
            break;

        futures.push_back(
            std::async(std::launch::async, [&, begin, end]() -> std::vector<SearchResult> {
                thread_local std::string tlsUtf8Buf;

                std::vector<SearchResult> local;
                for (uint64_t i = begin; i < end; ++i) {
                    if (cancelToken.load(std::memory_order_relaxed))
                        break;
                    if (collected.load(std::memory_order_relaxed) >= maxResults)
                        break;

                    const EntryMeta& m = meta[i];
                    if (m.deleted)
                        continue;

                    const wchar_t* targetData;
                    size_t targetLen;
                    if (options.matchPath) {
                        targetData = pathPool + m.pathOffset;
                        targetLen = m.pathLen;
                    } else {
                        targetData = pathPool + m.pathOffset + m.nameStart;
                        targetLen = static_cast<size_t>(m.pathLen - m.nameStart);
                    }

                    WideToUtf8(targetData, targetLen, tlsUtf8Buf);
                    const std::string& utf8Target = tlsUtf8Buf;

                    if (options.wholeWord) {
                        re2::StringPiece match;
                        if (!reCapture->ok() || !RE2::PartialMatch(utf8Target, *reCapture, &match))
                            continue;
                        size_t matchPos = static_cast<size_t>(match.data() - utf8Target.data());
                        if (!MatchesWholeWord(targetData, targetLen, matchPos, match.size()))
                            continue;
                    } else {
                        if (!RE2::PartialMatch(utf8Target, re))
                            continue;
                    }

                    local.push_back({static_cast<uint32_t>(i), 0u, 0u});
                    collected.fetch_add(1, std::memory_order_relaxed);
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

std::vector<SearchResult> SearchEngine::SearchSubstring(
    const std::wstring& query, const EntryMeta* meta, uint64_t entryCount,
    const wchar_t* nameLowerPool, const wchar_t* pathPool, const SearchOptions& options,
    uint32_t maxResults, const std::atomic<bool>& cancelToken) {
    std::wstring needle = options.caseSensitive ? query : [&] {
        std::wstring q = query;
        std::transform(q.begin(), q.end(), q.begin(), ::towlower);
        return q;
    }();

    const bool doTokenMatch = !options.caseSensitive && TokenMatcher::QueryHasSeparators(query);
    std::wstring lowercaseQuery;
    std::vector<std::wstring_view> sortedQueryTokens;
    if (doTokenMatch) {
        lowercaseQuery = needle;
        sortedQueryTokens = TokenMatcher::TokenizeView(lowercaseQuery);
        std::sort(sortedQueryTokens.begin(), sortedQueryTokens.end());
    }

    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    uint64_t chunkSize = (entryCount + numThreads - 1) / numThreads;

    // Shared early-exit counter: stops all threads once maxResults hits are collected.
    std::atomic<uint32_t> collected{0};

    std::vector<std::future<std::vector<SearchResult>>> futures;
    futures.reserve(numThreads);

    for (unsigned int t = 0; t < numThreads; ++t) {
        uint64_t begin = t * chunkSize;
        uint64_t end = std::min(begin + chunkSize, entryCount);
        if (begin >= entryCount)
            break;

        futures.push_back(
            std::async(std::launch::async, [&, begin, end]() -> std::vector<SearchResult> {
                thread_local std::wstring tlsPathBuf;

                std::vector<SearchResult> local;
                for (uint64_t i = begin; i < end; ++i) {
                    if (cancelToken.load(std::memory_order_relaxed))
                        break;
                    if (collected.load(std::memory_order_relaxed) >= maxResults)
                        break;

                    const EntryMeta& m = meta[i];
                    if (m.deleted)
                        continue;

                    const wchar_t* haystackData;
                    size_t haystackLen;

                    if (options.matchPath) {
                        if (options.caseSensitive) {
                            haystackData = pathPool + m.pathOffset;
                            haystackLen = m.pathLen;
                        } else {
                            tlsPathBuf.assign(pathPool + m.pathOffset, m.pathLen);
                            std::transform(tlsPathBuf.begin(), tlsPathBuf.end(), tlsPathBuf.begin(),
                                           ::towlower);
                            haystackData = tlsPathBuf.c_str();
                            haystackLen = tlsPathBuf.size();
                        }
                    } else if (options.caseSensitive) {
                        // Case-sensitive name: read original-case name from path tail
                        haystackData = pathPool + m.pathOffset + m.nameStart;
                        haystackLen = static_cast<size_t>(m.pathLen - m.nameStart);
                    } else {
                        // Hot path: pre-lowercased nameLower — zero allocation, pool-direct
                        haystackData = nameLowerPool + m.nameLowerOffset;
                        haystackLen = m.nameLowerLen;
                    }

                    size_t pos =
                        SimdFindSubstring(haystackData, haystackLen, needle.c_str(), needle.size());

                    bool tokenMatch = false;
                    if (pos == std::wstring::npos) {
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
                        if (!MatchesWholeWord(haystackData, haystackLen, pos, needle.size()))
                            continue;
                    }

                    local.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(pos),
                                     tokenMatch ? 0u : static_cast<uint32_t>(needle.size())});
                    collected.fetch_add(1, std::memory_order_relaxed);
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
