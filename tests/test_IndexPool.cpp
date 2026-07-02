#include <gtest/gtest.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "storage/IndexPool.h"
#include <algorithm>

using namespace winindex;

static FileEntry MakeEntry(const wchar_t* name, const wchar_t* path, uint64_t size = 0,
                           uint64_t modified = 0, uint32_t attrs = FILE_ATTRIBUTE_NORMAL) {
    FileEntry e;
    e.name = name;
    e.nameLower = name;
    std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
    e.path = path;
    e.size = size;
    e.lastModified = modified;
    e.attributes = attrs;
    return e;
}

TEST(IndexPoolTest, EmptyPool) {
    IndexPool pool;
    EXPECT_EQ(pool.Size(), 0u);
}

TEST(IndexPoolTest, AddOneEntry) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"report.txt", L"C:\\docs\\report.txt", 1024, 999));

    ASSERT_EQ(pool.Size(), 1u);
    EXPECT_EQ(pool.GetName(0), L"report.txt");
    EXPECT_EQ(pool.GetPath(0), L"C:\\docs\\report.txt");
    EXPECT_EQ(pool.GetNameLower(0), L"report.txt");
    EXPECT_EQ(pool.meta[0].size, 1024u);
    EXPECT_EQ(pool.meta[0].lastModified, 999u);
}

TEST(IndexPoolTest, NameLowerIsCaseInsensitive) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"REPORT.TXT", L"C:\\REPORT.TXT"));

    EXPECT_EQ(pool.GetNameLower(0), L"report.txt");
    EXPECT_EQ(pool.GetName(0), L"REPORT.TXT");
}

TEST(IndexPoolTest, NameLowerComputedWhenMissing) {
    IndexPool pool;
    FileEntry e;
    e.name = L"MixedCase.Pdf";
    e.nameLower = L"";  // intentionally empty — pool must compute it
    e.path = L"C:\\MixedCase.Pdf";
    pool.AddEntry(e);

    EXPECT_EQ(pool.GetNameLower(0), L"mixedcase.pdf");
    EXPECT_EQ(pool.GetName(0), L"MixedCase.Pdf");
}

TEST(IndexPoolTest, GetNameExtractsLastComponent) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"deep.log", L"C:\\a\\b\\c\\deep.log"));

    EXPECT_EQ(pool.GetName(0), L"deep.log");
    EXPECT_EQ(pool.GetPath(0), L"C:\\a\\b\\c\\deep.log");
}

TEST(IndexPoolTest, MultipleEntriesIndependent) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"alpha.txt", L"C:\\alpha.txt", 10));
    pool.AddEntry(MakeEntry(L"BETA.PDF", L"D:\\docs\\BETA.PDF", 20));
    pool.AddEntry(MakeEntry(L"gamma.mp3", L"E:\\music\\gamma.mp3", 30));

    ASSERT_EQ(pool.Size(), 3u);
    EXPECT_EQ(pool.GetName(0), L"alpha.txt");
    EXPECT_EQ(pool.GetNameLower(1), L"beta.pdf");
    EXPECT_EQ(pool.GetPath(2), L"E:\\music\\gamma.mp3");
    EXPECT_EQ(pool.meta[0].size, 10u);
    EXPECT_EQ(pool.meta[1].size, 20u);
    EXPECT_EQ(pool.meta[2].size, 30u);
}

TEST(IndexPoolTest, ClearResetsAll) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"file.txt", L"C:\\file.txt"));
    pool.Clear();

    EXPECT_EQ(pool.Size(), 0u);
    EXPECT_TRUE(pool.nameLowerPool.empty());
    EXPECT_TRUE(pool.pathPool.empty());
}

TEST(IndexPoolTest, TombstoneFlag) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"alive.txt", L"C:\\alive.txt"));
    pool.AddEntry(MakeEntry(L"dead.txt", L"C:\\dead.txt"));

    EXPECT_EQ(pool.meta[0].deleted, 0u);
    pool.meta[1].deleted = 1;
    EXPECT_EQ(pool.meta[1].deleted, 1u);
    EXPECT_EQ(pool.meta[0].deleted, 0u);  // first entry unaffected
}

TEST(IndexPoolTest, ReserveDoesNotChangeSize) {
    IndexPool pool;
    pool.Reserve(100000);
    EXPECT_EQ(pool.Size(), 0u);
    EXPECT_GE(pool.meta.capacity(), 100000u);
}

TEST(IndexPoolTest, PathAtRootLevel) {
    // No backslash in name (root of drive)
    IndexPool pool;
    FileEntry e;
    e.name = L"autorun.inf";
    e.nameLower = L"autorun.inf";
    e.path = L"C:\\autorun.inf";
    pool.AddEntry(e);

    EXPECT_EQ(pool.GetName(0), L"autorun.inf");
    EXPECT_EQ(pool.GetPath(0), L"C:\\autorun.inf");
}
