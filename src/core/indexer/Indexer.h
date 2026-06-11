#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "../settings/Settings.h"
#include "../storage/IIndexStore.h"
#include "ChangeWatcher.h"
#include "IFileSystemScanner.h"
#include "IUsnJournalMonitor.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace winindex {

enum class IndexerState { Idle, Scanning, LoadingIndex, WatchingForChanges, Error };

struct IndexerStatus {
    IndexerState state;
    std::wstring message;
    uint64_t filesIndexed;
    uint32_t skippedPaths;
};

using StatusCallback = std::function<void(const IndexerStatus&)>;

class Indexer {
public:
    Indexer(std::shared_ptr<IFileSystemScanner> mftScanner,
            std::shared_ptr<IFileSystemScanner> findScanner,
            std::shared_ptr<IUsnJournalMonitor> usnMonitor, std::shared_ptr<IIndexStore> indexStore,
            std::shared_ptr<Settings> settings);

    ~Indexer();

    void SetStatusCallback(StatusCallback cb);

    // Start indexing in background. Non-blocking.
    void StartIndexing(bool force = false);

    // Append-index new paths without clearing existing index. Non-blocking.
    void IndexPaths(std::vector<std::wstring> paths);

    // Remove all index entries under the given paths and save. Non-blocking.
    void RemovePaths(std::vector<std::wstring> paths);

    // Request cancellation of in-progress indexing.
    void Cancel();

    // Wait for indexing to complete (blocks).
    void WaitForCompletion();

    bool IsIndexing() const;

private:
    void IndexingThread();
    void IndexPathsThread(const std::vector<std::wstring>& paths);
    void RemovePathsThread(const std::vector<std::wstring>& paths);
    void ScanDrive(const std::wstring& root);
    void ApplyChange(const FileChangeEvent& evt);
    void StartLiveMonitoring();
    void StartWatchersForRoots(const std::vector<std::wstring>& roots);
    void StopWatchersForRoots(const std::vector<std::wstring>& roots);
    void EmitStatus(IndexerState state, std::wstring message, uint64_t filesIndexed = 0,
                    uint32_t skipped = 0);

    std::shared_ptr<IFileSystemScanner> m_mftScanner;
    std::shared_ptr<IFileSystemScanner> m_findScanner;
    std::shared_ptr<IUsnJournalMonitor> m_usnMonitor;
    std::shared_ptr<IIndexStore> m_indexStore;
    std::shared_ptr<Settings> m_settings;

    StatusCallback m_statusCallback;
    std::atomic<bool> m_cancel{false};
    HANDLE m_thread = nullptr;
    HANDLE m_completionEvent;
    uint64_t m_filesIndexed = 0;
    uint32_t m_skippedPaths = 0;
    std::mutex m_watchersMutex;
    std::vector<std::unique_ptr<ChangeWatcher>> m_watchers;
};

}  // namespace winindex
