#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "IFileSystemScanner.h"
#include "IUsnJournalMonitor.h"
#include "../storage/IIndexStore.h"
#include "../settings/Settings.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace winindex {

enum class IndexerState {
    Idle,
    Scanning,
    LoadingIndex,
    WatchingForChanges,
    Error
};

struct IndexerStatus {
    IndexerState state;
    std::wstring message;
    uint64_t     filesIndexed;
    uint32_t     skippedPaths;
};

using StatusCallback = std::function<void(const IndexerStatus&)>;

class Indexer {
public:
    Indexer(std::shared_ptr<IFileSystemScanner> mftScanner,
            std::shared_ptr<IFileSystemScanner> findScanner,
            std::shared_ptr<IUsnJournalMonitor> usnMonitor,
            std::shared_ptr<IIndexStore>        indexStore,
            std::shared_ptr<Settings>           settings);

    ~Indexer();

    void SetStatusCallback(StatusCallback cb);

    // Start indexing in background. Non-blocking.
    void StartIndexing(bool force = false);

    // Request cancellation of in-progress indexing.
    void Cancel();

    // Wait for indexing to complete (blocks).
    void WaitForCompletion();

    bool IsIndexing() const;

private:
    void IndexingThread();
    void ScanDrive(const std::wstring& root);
    void ApplyChange(const FileChangeEvent& evt);
    void EmitStatus(IndexerState state, std::wstring message,
                    uint64_t filesIndexed = 0, uint32_t skipped = 0);

    std::shared_ptr<IFileSystemScanner> m_mftScanner;
    std::shared_ptr<IFileSystemScanner> m_findScanner;
    std::shared_ptr<IUsnJournalMonitor> m_usnMonitor;
    std::shared_ptr<IIndexStore>        m_indexStore;
    std::shared_ptr<Settings>           m_settings;

    StatusCallback      m_statusCallback;
    std::atomic<bool>   m_cancel{false};
    HANDLE              m_thread = nullptr;
    HANDLE              m_completionEvent = nullptr;
    uint64_t            m_filesIndexed = 0;
    uint32_t            m_skippedPaths = 0;
};

} // namespace winindex
