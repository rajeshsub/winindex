#include <gtest/gtest.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

    void TearDown() override {
        _wremove(tmpPath.c_str());
    }
};

TEST_F(IndexSerializerTest, RoundtripEmptyIndex) {
    std::vector<FileEntry> entries;
    std::unordered_map<std::wstring, uint64_t> usnMap;

    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, entries, usnMap));

    std::vector<FileEntry> loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));
    EXPECT_EQ(loaded.size(), 0u);
    EXPECT_GT(ts, 0u);
}

TEST_F(IndexSerializerTest, RoundtripSingleEntry) {
    FileEntry e;
    e.name         = L"hello.txt";
    e.path         = L"C:\\Users\\test\\hello.txt";
    e.size         = 12345;
    e.lastModified = 999888777;
    e.attributes   = FILE_ATTRIBUTE_NORMAL;

    std::vector<FileEntry> entries = { e };
    std::unordered_map<std::wstring, uint64_t> usnMap;
    usnMap[L"C:\\"] = 42;

    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, entries, usnMap));

    std::vector<FileEntry> loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));

    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].name,         e.name);
    EXPECT_EQ(loaded[0].path,         e.path);
    EXPECT_EQ(loaded[0].size,         e.size);
    EXPECT_EQ(loaded[0].lastModified, e.lastModified);
    EXPECT_EQ(loaded[0].attributes,   e.attributes);
    EXPECT_EQ(loadedUsn[L"C:\\"],     42u);
}

TEST_F(IndexSerializerTest, RoundtripManyEntries) {
    std::vector<FileEntry> entries;
    for (int i = 0; i < 10000; ++i) {
        FileEntry e;
        e.name         = L"file_" + std::to_wstring(i) + L".dat";
        e.path         = L"C:\\data\\file_" + std::to_wstring(i) + L".dat";
        e.size         = static_cast<uint64_t>(i) * 1024;
        e.lastModified = static_cast<uint64_t>(i);
        e.attributes   = FILE_ATTRIBUTE_NORMAL;
        entries.push_back(e);
    }

    std::unordered_map<std::wstring, uint64_t> usnMap;
    ASSERT_TRUE(IndexSerializer::Serialize(tmpPath, entries, usnMap));

    std::vector<FileEntry> loaded;
    std::unordered_map<std::wstring, uint64_t> loadedUsn;
    uint64_t ts = 0;
    ASSERT_TRUE(IndexSerializer::Deserialize(tmpPath, loaded, loadedUsn, ts));
    EXPECT_EQ(loaded.size(), 10000u);
}

TEST_F(IndexSerializerTest, CorruptFileFails) {
    // Write garbage
    FILE* f = _wfopen(tmpPath.c_str(), L"wb");
    ASSERT_NE(f, nullptr);
    const char garbage[] = "this is not a valid index file at all!!!";
    fwrite(garbage, 1, sizeof(garbage), f);
    fclose(f);

    std::vector<FileEntry> loaded;
    std::unordered_map<std::wstring, uint64_t> usnMap;
    uint64_t ts = 0;
    EXPECT_FALSE(IndexSerializer::Deserialize(tmpPath, loaded, usnMap, ts));
}

TEST_F(IndexSerializerTest, MissingFileFails) {
    std::vector<FileEntry> loaded;
    std::unordered_map<std::wstring, uint64_t> usnMap;
    uint64_t ts = 0;
    EXPECT_FALSE(IndexSerializer::Deserialize(L"C:\\does_not_exist_winindex.idx",
                                               loaded, usnMap, ts));
}
