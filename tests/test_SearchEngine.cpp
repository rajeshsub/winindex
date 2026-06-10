#include <gtest/gtest.h>

#include "search/SearchEngine.h"
#include "search/SimdSearch.h"
#include "search/TokenMatcher.h"
#include <atomic>
#include <vector>

using namespace winindex;

static std::vector<FileEntry> MakeEntries(
    std::initializer_list<std::pair<const wchar_t*, const wchar_t*>> items) {
    std::vector<FileEntry> v;
    for (auto& [name, path] : items) {
        FileEntry e;
        e.name = name;
        e.nameLower = name;
        std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
        e.path = path;
        e.size = 0;
        e.lastModified = 0;
        e.attributes = 0;
        v.push_back(e);
    }
    return v;
}

class SearchEngineTest : public ::testing::Test {
protected:
    SearchEngine engine;
    std::atomic<bool> cancel{false};
};

TEST_F(SearchEngineTest, BasicSubstringMatch) {
    auto entries = MakeEntries({
        {L"report_2024.xlsx", L"C:\\docs\\report_2024.xlsx"},
        {L"summary.pdf", L"C:\\docs\\summary.pdf"},
        {L"report_q1.docx", L"C:\\docs\\report_q1.docx"},
    });

    SearchOptions opts{};
    auto results = engine.Search(L"report", entries.data(), entries.size(), opts, 100, cancel);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, CaseSensitiveMatch) {
    auto entries = MakeEntries({
        {L"Report.txt", L"C:\\Report.txt"},
        {L"report.txt", L"C:\\report.txt"},
    });

    SearchOptions opts{};
    opts.caseSensitive = true;
    auto results = engine.Search(L"Report", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"Report.txt");
}

TEST_F(SearchEngineTest, CaseInsensitiveMatch) {
    auto entries = MakeEntries({
        {L"REPORT.txt", L"C:\\REPORT.txt"},
        {L"report.txt", L"C:\\report.txt"},
    });

    SearchOptions opts{};
    opts.caseSensitive = false;
    auto results = engine.Search(L"report", entries.data(), entries.size(), opts, 100, cancel);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, WholeWordMatch) {
    auto entries = MakeEntries({
        {L"report.txt", L"C:\\report.txt"},
        {L"reports_final.txt", L"C:\\reports_final.txt"},
    });

    SearchOptions opts{};
    opts.wholeWord = true;
    auto results = engine.Search(L"report", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"report.txt");
}

TEST_F(SearchEngineTest, MatchPathOption) {
    auto entries = MakeEntries({
        {L"file.txt", L"C:\\Projects\\report\\file.txt"},
        {L"other.txt", L"C:\\Documents\\other.txt"},
    });

    SearchOptions opts{};
    opts.matchPath = true;
    auto results = engine.Search(L"report", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"file.txt");
}

TEST_F(SearchEngineTest, RegexMatch) {
    auto entries = MakeEntries({
        {L"invoice_001.pdf", L"C:\\invoice_001.pdf"},
        {L"invoice_abc.pdf", L"C:\\invoice_abc.pdf"},
        {L"summary.pdf", L"C:\\summary.pdf"},
    });

    SearchOptions opts{};
    opts.useRegex = true;
    auto results =
        engine.Search(L"invoice_\\d+", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"invoice_001.pdf");
}

TEST_F(SearchEngineTest, InvalidRegexReturnsEmpty) {
    auto entries = MakeEntries({
        {L"file.txt", L"C:\\file.txt"},
    });

    SearchOptions opts{};
    opts.useRegex = true;
    auto results =
        engine.Search(L"[invalid(regex", entries.data(), entries.size(), opts, 100, cancel);
    EXPECT_TRUE(results.empty());
}

TEST_F(SearchEngineTest, MaxResultsCap) {
    std::vector<FileEntry> entries;
    for (int i = 0; i < 200; ++i) {
        FileEntry e;
        e.name = L"file_" + std::to_wstring(i) + L".txt";
        e.path = L"C:\\" + e.name;
        entries.push_back(e);
    }

    SearchOptions opts{};
    auto results = engine.Search(L"fi", entries.data(), entries.size(), opts, 10, cancel);
    EXPECT_LE(results.size(), 10u);
}

TEST_F(SearchEngineTest, QueryTooShortReturnsEmpty) {
    auto entries = MakeEntries({{L"file.txt", L"C:\\file.txt"}});
    SearchOptions opts{};
    auto results = engine.Search(L"f", entries.data(), entries.size(), opts, 100, cancel);
    EXPECT_TRUE(results.empty());
}

TEST_F(SearchEngineTest, CancelTokenAbortsSearch) {
    std::vector<FileEntry> entries;
    for (int i = 0; i < 100000; ++i) {
        FileEntry e;
        e.name = L"somefile_" + std::to_wstring(i) + L".txt";
        e.path = L"C:\\" + e.name;
        entries.push_back(e);
    }

    std::atomic<bool> cancelNow{true};  // already cancelled
    SearchOptions opts{};
    auto results =
        engine.Search(L"somefile", entries.data(), entries.size(), opts, 10000, cancelNow);
    // With immediate cancel the result set should be small or empty
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
    std::vector<FileEntry> entries = MakeEntries({
        {kLedZepName, kLedZepPath},
        {L"unrelated_song.mp3", L"C:\\music\\unrelated_song.mp3"},
    });
    SearchOptions opts{};  // caseSensitive=false, useRegex=false
};

TEST_F(TokenSetMatchTest, SpaceSeparatorMatchesHyphen) {
    auto r = engine.Search(L"just rosy", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, UpperCaseQuerySpaceSep) {
    auto r = engine.Search(L"Just rosy", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, UnderscoreSeparatorMatchesHyphen) {
    auto r = engine.Search(L"just_rosy", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, MixedSeparatorsInQuery) {
    auto r = engine.Search(L"just rosy june", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, NonAdjacentTokens) {
    auto r = engine.Search(L"just rosy guitar", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, TokensFromDifferentParts) {
    auto r = engine.Search(L"rosy guitar flac", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, LedZepPlusRosy) {
    auto r = engine.Search(L"LedZep rosy", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, LedZepPlusFlac) {
    auto r = engine.Search(L"ledzep flac", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, TokenOrderIrrelevant_GuitarFirst) {
    auto r = engine.Search(L"just guitar rosy", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, TokenOrderIrrelevant_RosyFirst) {
    auto r = engine.Search(L"rosy just guitar", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, AllTokensMustMatch_NegativeCase) {
    // "piano" is not in the filename — should not match
    auto r = engine.Search(L"just rosy piano", entries.data(), entries.size(), opts, 100, cancel);
    EXPECT_EQ(r.size(), 0u);
}

TEST_F(TokenSetMatchTest, SingleWordQueryUsesSimdPath) {
    // "ledzep" is a direct substring — still found via SIMD path
    auto r = engine.Search(L"ledzep", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, ExactHyphenSubstringStillWorks) {
    // "just-rosy" is a literal substring — SIMD finds it without token path
    auto r = engine.Search(L"just-rosy", entries.data(), entries.size(), opts, 100, cancel);
    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(r[0].entry->name, kLedZepName);
}

TEST_F(TokenSetMatchTest, CaseSensitiveModeSkipsTokenPath) {
    SearchOptions caseSens{};
    caseSens.caseSensitive = true;
    // lowercase "just rosy" won't match mixed-case filename in case-sensitive mode
    auto r = engine.Search(L"just rosy", entries.data(), entries.size(), caseSens, 100, cancel);
    EXPECT_EQ(r.size(), 0u);
}

TEST_F(TokenSetMatchTest, PartialTokenDoesNotMatch) {
    // "led" and "zep" are not independent tokens in the filename ("ledzep" is one token)
    auto r = engine.Search(L"led zep", entries.data(), entries.size(), opts, 100, cancel);
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
    // We can't assert specific caps since it depends on host CPU,
    // but the call must not crash.
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
