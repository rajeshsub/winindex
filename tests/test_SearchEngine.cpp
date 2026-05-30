#include <gtest/gtest.h>
#include "search/SearchEngine.h"
#include "search/SimdSearch.h"
#include <atomic>
#include <vector>

using namespace winindex;

static std::vector<FileEntry> MakeEntries(
    std::initializer_list<std::pair<const wchar_t*, const wchar_t*>> items) {
    std::vector<FileEntry> v;
    for (auto& [name, path] : items) {
        FileEntry e;
        e.name = name;
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
        { L"report_2024.xlsx", L"C:\\docs\\report_2024.xlsx" },
        { L"summary.pdf",      L"C:\\docs\\summary.pdf"      },
        { L"report_q1.docx",   L"C:\\docs\\report_q1.docx"   },
    });

    SearchOptions opts{};
    auto results = engine.Search(L"report", entries.data(), entries.size(),
                                  opts, 100, cancel);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, CaseSensitiveMatch) {
    auto entries = MakeEntries({
        { L"Report.txt",  L"C:\\Report.txt"  },
        { L"report.txt",  L"C:\\report.txt"  },
    });

    SearchOptions opts{};
    opts.caseSensitive = true;
    auto results = engine.Search(L"Report", entries.data(), entries.size(),
                                  opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"Report.txt");
}

TEST_F(SearchEngineTest, CaseInsensitiveMatch) {
    auto entries = MakeEntries({
        { L"REPORT.txt", L"C:\\REPORT.txt" },
        { L"report.txt", L"C:\\report.txt" },
    });

    SearchOptions opts{};
    opts.caseSensitive = false;
    auto results = engine.Search(L"report", entries.data(), entries.size(),
                                  opts, 100, cancel);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(SearchEngineTest, WholeWordMatch) {
    auto entries = MakeEntries({
        { L"report.txt",        L"C:\\report.txt"        },
        { L"reports_final.txt", L"C:\\reports_final.txt" },
    });

    SearchOptions opts{};
    opts.wholeWord = true;
    auto results = engine.Search(L"report", entries.data(), entries.size(),
                                  opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"report.txt");
}

TEST_F(SearchEngineTest, MatchPathOption) {
    auto entries = MakeEntries({
        { L"file.txt", L"C:\\Projects\\report\\file.txt" },
        { L"other.txt", L"C:\\Documents\\other.txt"       },
    });

    SearchOptions opts{};
    opts.matchPath = true;
    auto results = engine.Search(L"report", entries.data(), entries.size(),
                                  opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"file.txt");
}

TEST_F(SearchEngineTest, RegexMatch) {
    auto entries = MakeEntries({
        { L"invoice_001.pdf", L"C:\\invoice_001.pdf" },
        { L"invoice_abc.pdf", L"C:\\invoice_abc.pdf" },
        { L"summary.pdf",     L"C:\\summary.pdf"     },
    });

    SearchOptions opts{};
    opts.useRegex = true;
    auto results = engine.Search(L"invoice_\\d+", entries.data(), entries.size(),
                                  opts, 100, cancel);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].entry->name, L"invoice_001.pdf");
}

TEST_F(SearchEngineTest, InvalidRegexReturnsEmpty) {
    auto entries = MakeEntries({
        { L"file.txt", L"C:\\file.txt" },
    });

    SearchOptions opts{};
    opts.useRegex = true;
    auto results = engine.Search(L"[invalid(regex", entries.data(), entries.size(),
                                  opts, 100, cancel);
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
    auto results = engine.Search(L"fi", entries.data(), entries.size(),
                                  opts, 10, cancel);
    EXPECT_LE(results.size(), 10u);
}

TEST_F(SearchEngineTest, QueryTooShortReturnsEmpty) {
    auto entries = MakeEntries({ { L"file.txt", L"C:\\file.txt" } });
    SearchOptions opts{};
    auto results = engine.Search(L"f", entries.data(), entries.size(),
                                  opts, 100, cancel);
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

    std::atomic<bool> cancelNow{true}; // already cancelled
    SearchOptions opts{};
    auto results = engine.Search(L"somefile", entries.data(), entries.size(),
                                  opts, 10000, cancelNow);
    // With immediate cancel the result set should be small or empty
    EXPECT_LT(results.size(), 10000u);
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
    size_t pos = winindex::SimdFindSubstring(hay.c_str(), hay.size(),
                                              needle.c_str(), needle.size());
    EXPECT_EQ(pos, 6u);
}

TEST(SimdTest, FindSubstringNotFound) {
    std::wstring hay    = L"hello world";
    std::wstring needle = L"xyz";
    size_t pos = winindex::SimdFindSubstring(hay.c_str(), hay.size(),
                                              needle.c_str(), needle.size());
    EXPECT_EQ(pos, std::wstring::npos);
}

TEST(SimdTest, FindSubstringAtStart) {
    std::wstring hay    = L"startmatch";
    std::wstring needle = L"start";
    EXPECT_EQ(winindex::SimdFindSubstring(
                  hay.c_str(), hay.size(), needle.c_str(), needle.size()), 0u);
}

TEST(SimdTest, FindSubstringAtEnd) {
    std::wstring hay    = L"helloend";
    std::wstring needle = L"end";
    EXPECT_EQ(winindex::SimdFindSubstring(
                  hay.c_str(), hay.size(), needle.c_str(), needle.size()), 5u);
}
