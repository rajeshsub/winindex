#include <gtest/gtest.h>

#include "search/SearchEngine.h"
#include "search/SimdSearch.h"
#include "search/TokenMatcher.h"
#include "storage/IndexPool.h"
#include <atomic>
#include <vector>

using namespace winindex;

static IndexPool MakePool(std::initializer_list<std::pair<const wchar_t*, const wchar_t*>> items) {
    IndexPool pool;
    for (auto& [name, path] : items) {
        FileEntry e;
        e.name = name;
        e.nameLower = name;
        std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
        e.path = path;
        e.size = 0;
        e.lastModified = 0;
        e.attributes = 0;
        pool.AddEntry(e);
    }
    return pool;
}

static std::vector<SearchResult> DoSearch(SearchEngine& engine, const IndexPool& pool,
                                          const std::wstring& query, const SearchOptions& opts,
                                          uint32_t maxResults, const std::atomic<bool>& cancel) {
    return engine.Search(query, pool.meta.data(), static_cast<uint64_t>(pool.meta.size()),
                         pool.nameLowerPool.data(), pool.pathPool.data(), opts, maxResults, cancel);
}

class SearchEngineTest : public ::testing::Test {
protected:
    SearchEngine engine;
    std::atomic<bool> cancel{false};
};

TEST_F(SearchEngineTest, BasicSubstringMatch) {
    auto pool = MakePool({
        {L"report_2024.xlsx", L"C:\\docs\\report_2024.xlsx"},
        {L"summary.pdf", L"C:\\docs\\summary.pdf"},
        {L"report_q1.docx", L"C:\\docs\\report_q1.docx"},
    });

    SearchOptions opts{};
    auto results = DoSearch(engine, pool, L"report", opts, 100, cancel);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, CaseSensitiveMatch) {
    auto pool = MakePool({
        {L"Report.txt", L"C:\\Report.txt"},
        {L"report.txt", L"C:\\report.txt"},
    });

    SearchOptions opts{};
    opts.caseSensitive = true;
    auto results = DoSearch(engine, pool, L"Report", opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(pool.GetName(results[0].entryIndex), L"Report.txt");
}

TEST_F(SearchEngineTest, CaseInsensitiveMatch) {
    auto pool = MakePool({
        {L"REPORT.txt", L"C:\\REPORT.txt"},
        {L"report.txt", L"C:\\report.txt"},
    });

    SearchOptions opts{};
    opts.caseSensitive = false;
    auto results = DoSearch(engine, pool, L"report", opts, 100, cancel);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, WholeWordMatch) {
    auto pool = MakePool({
        {L"report.txt", L"C:\\report.txt"},
        {L"reports_final.txt", L"C:\\reports_final.txt"},
    });

    SearchOptions opts{};
    opts.wholeWord = true;
    auto results = DoSearch(engine, pool, L"report", opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(pool.GetName(results[0].entryIndex), L"report.txt");
}

TEST_F(SearchEngineTest, MatchPathOption) {
    auto pool = MakePool({
        {L"file.txt", L"C:\\Projects\\report\\file.txt"},
        {L"other.txt", L"C:\\Documents\\other.txt"},
    });

    SearchOptions opts{};
    opts.matchPath = true;
    auto results = DoSearch(engine, pool, L"report", opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(pool.GetName(results[0].entryIndex), L"file.txt");
}

TEST_F(SearchEngineTest, RegexMatch) {
    auto pool = MakePool({
        {L"invoice_001.pdf", L"C:\\invoice_001.pdf"},
        {L"invoice_abc.pdf", L"C:\\invoice_abc.pdf"},
        {L"summary.pdf", L"C:\\summary.pdf"},
    });

    SearchOptions opts{};
    opts.useRegex = true;
    auto results = DoSearch(engine, pool, L"invoice_\\d+", opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(pool.GetName(results[0].entryIndex), L"invoice_001.pdf");
}

TEST_F(SearchEngineTest, InvalidRegexReturnsEmpty) {
    auto pool = MakePool({{L"file.txt", L"C:\\file.txt"}});

    SearchOptions opts{};
    opts.useRegex = true;
    auto results = DoSearch(engine, pool, L"[invalid(regex", opts, 100, cancel);
    EXPECT_TRUE(results.empty());
}

TEST_F(SearchEngineTest, MaxResultsCap) {
    IndexPool pool;
    for (int i = 0; i < 200; ++i) {
        FileEntry e;
        e.name = L"file_" + std::to_wstring(i) + L".txt";
        e.path = L"C:\\" + e.name;
        e.nameLower = e.name;
        std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
        pool.AddEntry(e);
    }

    SearchOptions opts{};
    auto results = DoSearch(engine, pool, L"fi", opts, 10, cancel);
    EXPECT_LE(results.size(), 10u);
}

TEST_F(SearchEngineTest, QueryTooShortReturnsEmpty) {
    auto pool = MakePool({{L"file.txt", L"C:\\file.txt"}});
    SearchOptions opts{};
    auto results = DoSearch(engine, pool, L"f", opts, 100, cancel);
    EXPECT_TRUE(results.empty());
}

TEST_F(SearchEngineTest, CancelTokenAbortsSearch) {
    IndexPool pool;
    for (int i = 0; i < 100000; ++i) {
        FileEntry e;
        e.name = L"somefile_" + std::to_wstring(i) + L".txt";
        e.path = L"C:\\" + e.name;
        e.nameLower = e.name;
        std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
        pool.AddEntry(e);
    }

    std::atomic<bool> cancelNow{true};  // already cancelled
    SearchOptions opts{};
    auto results = DoSearch(engine, pool, L"somefile", opts, 10000, cancelNow);
    EXPECT_LT(results.size(), 10000u);
}

// ---------------------------------------------------------------------------
// Token-set matching tests
// ---------------------------------------------------------------------------

static const wchar_t* kLedZepName = L"LedZep_Just-Rosy_June-Bug_guitar_4082.flac";
static const wchar_t* kLedZepPath = L"C:\\music\\LedZep_Just-Rosy_June-Bug_guitar_4082.flac";

class TokenSetMatchTest : public ::testing::Test {
protected:
    SearchEngine engine;
    std::atomic<bool> cancel{false};
    IndexPool pool = MakePool({
        {kLedZepName, kLedZepPath},
        {L"unrelated_song.mp3", L"C:\\music\\unrelated_song.mp3"},
    });
    SearchOptions opts{};  // caseSensitive=false, useRegex=false
};

TEST_F(TokenSetMatchTest, SpaceSeparatorMatchesHyphen) {
    auto r = DoSearch(engine, pool, L"just rosy", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, UpperCaseQuerySpaceSep) {
    auto r = DoSearch(engine, pool, L"Just rosy", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, UnderscoreSeparatorMatchesHyphen) {
    auto r = DoSearch(engine, pool, L"just_rosy", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, MixedSeparatorsInQuery) {
    auto r = DoSearch(engine, pool, L"just rosy june", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, NonAdjacentTokens) {
    auto r = DoSearch(engine, pool, L"just rosy guitar", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, TokensFromDifferentParts) {
    auto r = DoSearch(engine, pool, L"rosy guitar flac", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, LedZepPlusRosy) {
    auto r = DoSearch(engine, pool, L"LedZep rosy", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, LedZepPlusFlac) {
    auto r = DoSearch(engine, pool, L"ledzep flac", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, TokenOrderIrrelevant_GuitarFirst) {
    auto r = DoSearch(engine, pool, L"just guitar rosy", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, TokenOrderIrrelevant_RosyFirst) {
    auto r = DoSearch(engine, pool, L"rosy just guitar", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, AllTokensMustMatch_NegativeCase) {
    auto r = DoSearch(engine, pool, L"just rosy piano", opts, 100, cancel);
    EXPECT_EQ(r.size(), 0u);
}

TEST_F(TokenSetMatchTest, SingleWordQueryUsesSimdPath) {
    auto r = DoSearch(engine, pool, L"ledzep", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, ExactHyphenSubstringStillWorks) {
    auto r = DoSearch(engine, pool, L"just-rosy", opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(pool.GetName(r[0].entryIndex), kLedZepName);
}

TEST_F(TokenSetMatchTest, CaseSensitiveModeSkipsTokenPath) {
    SearchOptions caseSens{};
    caseSens.caseSensitive = true;
    auto r = DoSearch(engine, pool, L"just rosy", caseSens, 100, cancel);
    EXPECT_EQ(r.size(), 0u);
}

TEST_F(TokenSetMatchTest, PartialTokenDoesNotMatch) {
    auto r = DoSearch(engine, pool, L"led zep", opts, 100, cancel);
    EXPECT_EQ(r.size(), 0u);
}

// ---------------------------------------------------------------------------
// TokenMatcher unit tests
// ---------------------------------------------------------------------------

TEST(TokenMatcherTest, TokenizeBasicSplit) {
    std::wstring s = L"just-rosy_june";
    auto tokens = TokenMatcher::TokenizeView(s);
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], L"just");
    EXPECT_EQ(tokens[1], L"rosy");
    EXPECT_EQ(tokens[2], L"june");
}

TEST(TokenMatcherTest, TokenizeConsecutiveSepsSkipped) {
    std::wstring s = L"a--b";
    auto tokens = TokenMatcher::TokenizeView(s);
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], L"a");
    EXPECT_EQ(tokens[1], L"b");
}

TEST(TokenMatcherTest, TokenizeNoSepReturnsSingle) {
    std::wstring s = L"ledzep";
    auto tokens = TokenMatcher::TokenizeView(s);
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0], L"ledzep");
}

TEST(TokenMatcherTest, TokenizeDotHandled) {
    std::wstring s = L"song.flac";
    auto tokens = TokenMatcher::TokenizeView(s);
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0], L"song");
    EXPECT_EQ(tokens[1], L"flac");
}

TEST(TokenMatcherTest, QueryHasSeparators_True) {
    EXPECT_TRUE(TokenMatcher::QueryHasSeparators(L"just rosy"));
    EXPECT_TRUE(TokenMatcher::QueryHasSeparators(L"just_rosy"));
    EXPECT_TRUE(TokenMatcher::QueryHasSeparators(L"just-rosy"));
}

TEST(TokenMatcherTest, QueryHasSeparators_False) {
    EXPECT_FALSE(TokenMatcher::QueryHasSeparators(L"ledzep"));
    EXPECT_FALSE(TokenMatcher::QueryHasSeparators(L"guitar"));
}

TEST(TokenMatcherTest, AllQueryTokensPresent_AllMatch) {
    std::wstring q = L"just rosy";
    auto qt = TokenMatcher::TokenizeView(q);
    std::sort(qt.begin(), qt.end());
    std::wstring fn = L"ledzep just rosy june bug guitar 4082 flac";
    auto ft = TokenMatcher::TokenizeView(fn);
    std::sort(ft.begin(), ft.end());
    EXPECT_TRUE(TokenMatcher::AllQueryTokensPresent(qt, ft));
}

TEST(TokenMatcherTest, AllQueryTokensPresent_OneMissing) {
    std::wstring q = L"just piano";
    auto qt = TokenMatcher::TokenizeView(q);
    std::sort(qt.begin(), qt.end());
    std::wstring fn = L"ledzep just rosy june";
    auto ft = TokenMatcher::TokenizeView(fn);
    std::sort(ft.begin(), ft.end());
    EXPECT_FALSE(TokenMatcher::AllQueryTokensPresent(qt, ft));
}

TEST(TokenMatcherTest, AllQueryTokensPresent_EmptyQueryReturnsFalse) {
    std::vector<std::wstring_view> emptyQ;
    std::wstring fn = L"ledzep just rosy";
    auto ft = TokenMatcher::TokenizeView(fn);
    std::sort(ft.begin(), ft.end());
    EXPECT_FALSE(TokenMatcher::AllQueryTokensPresent(emptyQ, ft));
}

// SIMD detection test
TEST(SimdTest, DetectCaps) {
    auto caps = winindex::DetectSimdCaps();
    (void)caps;
}

TEST(SimdTest, FindSubstringBasic) {
    std::wstring hay = L"hello world foo";
    std::wstring needle = L"world";
    size_t pos =
        winindex::SimdFindSubstring(hay.c_str(), hay.size(), needle.c_str(), needle.size());
    EXPECT_EQ(pos, 6u);
}

TEST(SimdTest, FindSubstringNotFound) {
    std::wstring hay = L"hello world";
    std::wstring needle = L"xyz";
    size_t pos =
        winindex::SimdFindSubstring(hay.c_str(), hay.size(), needle.c_str(), needle.size());
    EXPECT_EQ(pos, std::wstring::npos);
}

TEST(SimdTest, FindSubstringAtStart) {
    std::wstring hay = L"startmatch";
    std::wstring needle = L"start";
    EXPECT_EQ(winindex::SimdFindSubstring(hay.c_str(), hay.size(), needle.c_str(), needle.size()),
              0u);
}

TEST(SimdTest, FindSubstringAtEnd) {
    std::wstring hay = L"helloend";
    std::wstring needle = L"end";
    EXPECT_EQ(winindex::SimdFindSubstring(hay.c_str(), hay.size(), needle.c_str(), needle.size()),
              5u);
}
