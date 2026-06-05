#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "indexer/Indexer.h"
#include "mocks/MockFileSystemScanner.h"
#include "mocks/MockIndexStore.h"
#include "mocks/MockUsnJournalMonitor.h"
#include "settings/Settings.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using namespace winindex;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;

class IndexerTest : public ::testing::Test {
protected:
    std::shared_ptr<MockFileSystemScanner> mftScanner;
    std::shared_ptr<MockFileSystemScanner> findScanner;
    std::shared_ptr<MockUsnJournalMonitor> usnMonitor;
    std::shared_ptr<MockIndexStore> indexStore;
    std::shared_ptr<Settings> settings;
    std::unique_ptr<Indexer> indexer;

    std::wstring tmpDir;

    void SetUp() override {
        wchar_t tmp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tmp);
        tmpDir = std::wstring(tmp) + L"winindex_indexer_test";
        CreateDirectoryW(tmpDir.c_str(), nullptr);

        mftScanner = std::make_shared<MockFileSystemScanner>();
        findScanner = std::make_shared<MockFileSystemScanner>();
        usnMonitor = std::make_shared<MockUsnJournalMonitor>();
        indexStore = std::make_shared<MockIndexStore>();
        settings = std::make_shared<Settings>(true, tmpDir);
        settings->Load();
        settings->SetSelectedDrives({L"C:\\"});

        indexer =
            std::make_unique<Indexer>(mftScanner, findScanner, usnMonitor, indexStore, settings);
    }

    void TearDown() override {
        indexer.reset();
        RemoveDirectoryW(tmpDir.c_str());
    }
};

TEST_F(IndexerTest, LoadsExistingValidIndex) {
    EXPECT_CALL(*indexStore, IsIndexValid()).WillOnce(Return(true));
    EXPECT_CALL(*indexStore, Load()).Times(1);
    EXPECT_CALL(*indexStore, GetEntryCount()).WillRepeatedly(Return(42));
    // Should NOT call scanner when index is valid
    EXPECT_CALL(*mftScanner, Scan(_, _, _, _)).Times(0);
    EXPECT_CALL(*findScanner, Scan(_, _, _, _)).Times(0);

    indexer->StartIndexing(false);
    indexer->WaitForCompletion();
}

TEST_F(IndexerTest, FullScanWhenIndexInvalid) {
    EXPECT_CALL(*indexStore, IsIndexValid()).WillOnce(Return(false));
    EXPECT_CALL(*indexStore, BeginWrite()).Times(1);
    EXPECT_CALL(*indexStore, EndWrite()).Times(1);
    EXPECT_CALL(*indexStore, Save()).Times(1);
    EXPECT_CALL(*indexStore, AddEntry(_)).Times(AtLeast(0));

    // MFT not available → use findScanner
    EXPECT_CALL(*mftScanner, IsMftAvailable(std::wstring(L"C:\\"))).WillOnce(Return(false));
    EXPECT_CALL(*findScanner, IsMftAvailable(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(*findScanner, Scan(_, _, _, _))
        .WillOnce(Invoke(
            [](const ScanOptions&, ScanCallback cb, ProgressCallback, const std::atomic<bool>&) {
                FileEntry e;
                e.name = L"test.txt";
                e.path = L"C:\\test.txt";
                e.size = 100;
                cb(e);
            }));

    EXPECT_CALL(*indexStore, AddEntry(_)).Times(1);

    indexer->StartIndexing(false);
    indexer->WaitForCompletion();
}

TEST_F(IndexerTest, ForceRebuildIgnoresValidIndex) {
    EXPECT_CALL(*indexStore, IsIndexValid()).Times(0);  // force=true skips check
    EXPECT_CALL(*indexStore, BeginWrite()).Times(1);
    EXPECT_CALL(*indexStore, EndWrite()).Times(1);
    EXPECT_CALL(*indexStore, Save()).Times(1);

    EXPECT_CALL(*mftScanner, IsMftAvailable(std::wstring(L"C:\\"))).WillOnce(Return(false));
    EXPECT_CALL(*findScanner, Scan(_, _, _, _)).Times(1);
    EXPECT_CALL(*indexStore, AddEntry(_)).Times(0);

    indexer->StartIndexing(true /*force*/);
    indexer->WaitForCompletion();
}

TEST_F(IndexerTest, StatusCallbackFired) {
    EXPECT_CALL(*indexStore, IsIndexValid()).WillOnce(Return(false));
    EXPECT_CALL(*indexStore, BeginWrite()).Times(1);
    EXPECT_CALL(*indexStore, EndWrite()).Times(1);
    EXPECT_CALL(*indexStore, Save()).Times(1);
    EXPECT_CALL(*mftScanner, IsMftAvailable(std::wstring(L"C:\\"))).WillOnce(Return(false));
    EXPECT_CALL(*findScanner, Scan(_, _, _, _)).Times(1);

    std::vector<IndexerState> states;
    indexer->SetStatusCallback([&](const IndexerStatus& s) { states.push_back(s.state); });

    indexer->StartIndexing(false);
    indexer->WaitForCompletion();

    EXPECT_FALSE(states.empty());
}
