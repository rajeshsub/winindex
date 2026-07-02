#include <gtest/gtest.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "storage/IndexPool.h"
#include "storage/IndexSerializer.h"
#include <filesystem>
#include <unordered_map>

using namespace winindex;
namespace fs = std::filesystem;

class IndexSerializerTest : public ::testing::Test {
protected:
    std::wstring tmpPath;

    void SetUp() override {
        wchar_t tmp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tmp);
        tmpPath = std::wstring(tmp) + L"winindex_test.idx";
    }

    void TearDown() override { _wremove(tmpPath.c_str()); }

    static FileEntry MakeEntry(const wchar_t* path, uint64_t size = 0, uint64_t lastModified = 0,
                               uint32_t attributes = 0) {
        FileEntry e;
        e.path = path;
        size_t slash = e.path.rfind(L'\\');
        e.name = (slash != std::wstring::npos) ? e.path.substr(slash + 1) : e.path;
        e.nameLower = e.name;
        std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
        e.size = size;
        e.lastModified = lastModified;
        e.attributes = attributes;
        return e;
    }
};

TEST_F(IndexSerializerTest, RoundtripEmptyIndex) {
    IndexPool pool;
    std::unordered_map<std::wstring, uint64_t> usnMap;

    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, pool, usnMap));

    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));
    EXPECT_EQ(loaded.meta.size(), 0u);
    EXPECT_GT(ts, 0u);
}

TEST_F(IndexSerializerTest, RoundtripSingleEntry) {
    IndexPool pool;
    pool.AddEntry(
        MakeEntry(L"C:\\Users\\test\\hello.txt", 12345, 999888777, FILE_ATTRIBUTE_NORMAL));

    std::unordered_map<std::wstring, uint64_t> usnMap;
    usnMap[L"C:\\"] = 42;

    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, pool, usnMap));

    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));

    ASSERT_EQ(loaded.meta.size(), 1u);
    EXPECT_EQ(loaded.GetName(0), L"hello.txt");
    EXPECT_EQ(loaded.GetPath(0), L"C:\\Users\\test\\hello.txt");
    EXPECT_EQ(loaded.meta[0].size, 12345u);
    EXPECT_EQ(loaded.meta[0].lastModified, 999888777u);
    EXPECT_EQ(loaded.meta[0].attributes, static_cast<uint32_t>(FILE_ATTRIBUTE_NORMAL));
    EXPECT_EQ(loadedUsn[L"C:\\"], 42u);
    EXPECT_GT(ts, 0u);
}

TEST_F(IndexSerializerTest, RoundtripManyEntries) {
    IndexPool pool;
    for (int i = 0; i < 10000; ++i) {
        pool.AddEntry(MakeEntry((L"C:\\data\\file_" + std::to_wstring(i) + L".dat").c_str(),
                                static_cast<uint64_t>(i) * 1024, static_cast<uint64_t>(i),
                                FILE_ATTRIBUTE_NORMAL));
    }

    std::unordered_map<std::wstring, uint64_t> usnMap;
    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, pool, usnMap));

    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));
    EXPECT_EQ(loaded.meta.size(), 10000u);
}

TEST_F(IndexSerializerTest, NameLowerRebuiltCorrectly) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"C:\\Docs\\Report_2024.xlsx"));

    std::unordered_map<std::wstring, uint64_t> usnMap;
    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, pool, usnMap));

    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));

    ASSERT_EQ(loaded.meta.size(), 1u);
    EXPECT_EQ(loaded.GetNameLower(0), L"report_2024.xlsx");
    EXPECT_EQ(loaded.GetName(0), L"Report_2024.xlsx");
}

TEST_F(IndexSerializerTest, TombstonedEntriesNotPersisted) {
    IndexPool pool;
    pool.AddEntry(MakeEntry(L"C:\\keep.txt", 100));
    pool.AddEntry(MakeEntry(L"C:\\delete.txt", 200));
    pool.meta[1].deleted = 1;

    std::unordered_map<std::wstring, uint64_t> usnMap;
    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, pool, usnMap));

    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));

    ASSERT_EQ(loaded.meta.size(), 1u);
    EXPECT_EQ(loaded.GetName(0), L"keep.txt");
    EXPECT_EQ(loaded.meta[0].size, 100u);
}

TEST_F(IndexSerializerTest, UsnMapRoundtrip) {
    IndexPool pool;
    std::unordered_map<std::wstring, uint64_t> usnMap;
    usnMap[L"C:\\"] = 111;
    usnMap[L"D:\\"] = 222;

    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, pool, usnMap));

    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));

    EXPECT_EQ(loadedUsn[L"C:\\"], 111u);
    EXPECT_EQ(loadedUsn[L"D:\\"], 222u);
}

TEST_F(IndexSerializerTest, CorruptFileFails) {
    FILE* f = _wfopen(tmpPath.c_str(), L"wb");
    ASSERT_NE(f, nullptr);
    const char garbage[] = "this is not a valid index file at all!!!";
    fwrite(garbage, 1, sizeof(garbage), f);
    fclose(f);

    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> usnMap;
    uint64_t ts = 0;
    EXPECT_FALSE(IndexSerializer::Deserialize(tmpPath, loaded, usnMap, ts));
}

TEST_F(IndexSerializerTest, MissingFileFails) {
    IndexPool loaded;
    std::unordered_map<std::wstring, uint64_t> usnMap;
    uint64_t ts = 0;
    EXPECT_FALSE(
        IndexSerializer::Deserialize(L"C:\\does_not_exist_winindex.idx", loaded, usnMap, ts));
}
