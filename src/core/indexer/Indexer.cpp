#include "Indexer.h"

#include "DriveEnumerator.h"
#include "MftScanner.h"
#include "UsnJournalMonitor.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <iterator>

namespace winindex {

Indexer::Indexer(std::shared_ptr<IFileSystemScanner> mftScanner,
                 std::shared_ptr<IFileSystemScanner> findScanner,
                 std::shared_ptr<IUsnJournalMonitor> usnMonitor,
                 std::shared_ptr<IIndexStore> indexStore, std::shared_ptr<Settings> settings)
    : m_mftScanner(std::move(mftScanner)),
      m_findScanner(std::move(findScanner)),
      m_usnMonitor(std::move(usnMonitor)),
      m_indexStore(std::move(indexStore)),
      m_settings(std::move(settings)),
      m_completionEvent(CreateEventW(nullptr, TRUE, TRUE, nullptr)) {}

Indexer::~Indexer() {
    Cancel();
    WaitForCompletion();
    if (m_thread)
        CloseHandle(m_thread);
    if (m_completionEvent)
        CloseHandle(m_completionEvent);
}

void Indexer::SetStatusCallback(StatusCallback cb) {
    m_statusCallback = std::move(cb);
}

void Indexer::StartIndexing(bool force) {
    if (IsIndexing())
        return;

    // Check if we need to rebuild
    if (!force && m_indexStore->IsIndexValid()) {
        EmitStatus(IndexerState::LoadingIndex, L"Loading index from disk...");
        m_indexStore->Load();
        if (m_indexStore->GetEntryCount() > 0) {
            EmitStatus(IndexerState::WatchingForChanges,
                       L"Index loaded - " + std::to_wstring(m_indexStore->GetEntryCount()) +
                           L" files indexed.");
            StartLiveMonitoring();
            return;
        }
        // Index file existed but was empty or corrupt — fall through to rebuild.
        EmitStatus(IndexerState::Scanning, L"Index empty or corrupt - rebuilding...");
    }

    m_cancel.store(false);
    ResetEvent(m_completionEvent);
    m_filesIndexed = 0;
    m_skippedPaths = 0;

    struct Params {
        Indexer* self;
    };
    auto* p = new Params{this};
    m_thread = CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* p = static_cast<Params*>(param);
            p->self->IndexingThread();
            delete p;
            return 0;
        },
        p, 0, nullptr);
}

void Indexer::IndexPaths(std::vector<std::wstring> paths) {
    if (IsIndexing())
        return;

    m_cancel.store(false);
    ResetEvent(m_completionEvent);
    m_filesIndexed = 0;
    m_skippedPaths = 0;

    struct Params {
        Indexer* self;
        std::vector<std::wstring> paths;
    };
    auto* p = new Params{this, std::move(paths)};
    m_thread = CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* p = static_cast<Params*>(param);
            p->self->IndexPathsThread(p->paths);
            delete p;
            return 0;
        },
        p, 0, nullptr);
}

void Indexer::IndexPathsThread(const std::vector<std::wstring>& paths) {
    EmitStatus(IndexerState::Scanning, L"Indexing new paths...");
    for (const auto& root : paths) {
        if (m_cancel.load(std::memory_order_relaxed))
            break;
        ScanDrive(root);
    }
    if (!m_cancel.load(std::memory_order_relaxed)) {
        m_indexStore->Save();
        EmitStatus(
            IndexerState::WatchingForChanges,
            L"Done - " + std::to_wstring(m_indexStore->GetEntryCount()) + L" files indexed.");
        StartWatchersForRoots(paths);
    }
    SetEvent(m_completionEvent);
}

void Indexer::RemovePaths(std::vector<std::wstring> paths) {
    if (IsIndexing())
        return;

    m_cancel.store(false);
    ResetEvent(m_completionEvent);

    struct Params {
        Indexer* self;
        std::vector<std::wstring> paths;
    };
    auto* p = new Params{this, std::move(paths)};
    m_thread = CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* p = static_cast<Params*>(param);
            p->self->RemovePathsThread(p->paths);
            delete p;
            return 0;
        },
        p, 0, nullptr);
}

void Indexer::RemovePathsThread(const std::vector<std::wstring>& paths) {
    EmitStatus(IndexerState::Scanning, L"Removing paths from index...");
    for (const auto& root : paths) m_indexStore->RemoveEntriesUnderPath(root);
    StopWatchersForRoots(paths);
    m_indexStore->Save();
    EmitStatus(IndexerState::WatchingForChanges,
               L"Done - " + std::to_wstring(m_indexStore->GetEntryCount()) + L" files indexed.");
    SetEvent(m_completionEvent);
}

void Indexer::StopWatchersForRoots(const std::vector<std::wstring>& roots) {
    std::lock_guard<std::mutex> lock(m_watchersMutex);
    std::vector<std::unique_ptr<ChangeWatcher>> kept;
    for (auto& w : m_watchers) {
        bool matched = std::any_of(roots.begin(), roots.end(),
                                   [&](const std::wstring& r) { return w->Root() == r; });
        if (matched) {
            w->Stop();
        } else {
            kept.push_back(std::move(w));
        }
    }
    m_watchers = std::move(kept);
}

void Indexer::Cancel() {
    m_cancel.store(true);
    std::lock_guard<std::mutex> lock(m_watchersMutex);
    for (auto& w : m_watchers) w->Stop();
    m_watchers.clear();
}

void Indexer::WaitForCompletion() {
    if (m_completionEvent)
        WaitForSingleObject(m_completionEvent, INFINITE);
}

bool Indexer::IsIndexing() const {
    if (!m_completionEvent)
        return false;
    return WaitForSingleObject(m_completionEvent, 0) == WAIT_TIMEOUT;
}

void Indexer::EmitStatus(IndexerState state, std::wstring message, uint64_t filesIndexed,
                         uint32_t skipped) {
    if (m_statusCallback) {
        IndexerStatus s{state, std::move(message), filesIndexed, skipped};
        m_statusCallback(s);
    }
}

void Indexer::IndexingThread() {
    EmitStatus(IndexerState::Scanning, L"Starting index build...");
    m_indexStore->BeginWrite();

    auto selectedDrives = m_settings->GetSelectedDrives();
    if (selectedDrives.empty()) {
        // No drives configured yet (e.g., first-run dialog was skipped).
        // Fall back to all local fixed drives so the app works out of the box.
        auto fixed = EnumerateLocalFixedDrives();
        std::transform(fixed.begin(), fixed.end(), std::back_inserter(selectedDrives),
                       [](const DriveInfo& d) { return d.root; });
    }
    for (const auto& root : selectedDrives) {
        if (m_cancel.load(std::memory_order_relaxed))
            break;
        ScanDrive(root);
    }

    if (!m_cancel.load(std::memory_order_relaxed)) {
        m_indexStore->EndWrite();
        m_indexStore->Save();
        EmitStatus(
            IndexerState::WatchingForChanges,
            L"Index built - " + std::to_wstring(m_filesIndexed) + L" files" +
                (m_skippedPaths > 0 ? L", " + std::to_wstring(m_skippedPaths) + L" paths skipped"
                                    : L"") +
                L".");
    }

    if (!m_cancel.load(std::memory_order_relaxed))
        StartLiveMonitoring();
    SetEvent(m_completionEvent);
}

void Indexer::ApplyChange(const FileChangeEvent& evt) {
    if (evt.type == FileChangeType::Added || evt.type == FileChangeType::Modified) {
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (!GetFileAttributesExW(evt.path.c_str(), GetFileExInfoStandard, &fad))
            return;
        if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return;
        if (evt.type == FileChangeType::Modified)
            m_indexStore->ApplyRemove(evt.path);
        FileEntry fe;
        size_t slash = evt.path.rfind(L'\\');
        fe.name = (slash != std::wstring::npos) ? evt.path.substr(slash + 1) : evt.path;
        fe.nameLower = fe.name;
        std::transform(fe.nameLower.begin(), fe.nameLower.end(), fe.nameLower.begin(), ::towlower);
        fe.path = evt.path;
        fe.size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
        fe.lastModified = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
                          fad.ftLastWriteTime.dwLowDateTime;
        fe.attributes = fad.dwFileAttributes;
        m_indexStore->ApplyAdd(fe);
    } else if (evt.type == FileChangeType::Removed) {
        m_indexStore->ApplyRemove(evt.path);
    }
}

void Indexer::StartLiveMonitoring() {
    auto drives = m_settings->GetSelectedDrives();
    if (drives.empty()) {
        auto fixed = EnumerateLocalFixedDrives();
        std::transform(fixed.begin(), fixed.end(), std::back_inserter(drives),
                       [](const DriveInfo& d) { return d.root; });
    }
    StartWatchersForRoots(drives);
}

void Indexer::StartWatchersForRoots(const std::vector<std::wstring>& roots) {
    for (const auto& root : roots) {
        if (m_cancel.load(std::memory_order_relaxed))
            return;
        std::lock_guard<std::mutex> lock(m_watchersMutex);
        bool exists =
            std::any_of(m_watchers.begin(), m_watchers.end(),
                        [&](const std::unique_ptr<ChangeWatcher>& w) { return w->Root() == root; });
        if (exists)
            continue;
        auto watcher = std::make_unique<ChangeWatcher>(root);
        watcher->Start([this](const FileChangeEvent& evt) { ApplyChange(evt); });
        m_watchers.push_back(std::move(watcher));
    }
}

void Indexer::ScanDrive(const std::wstring& root) {
    DriveFilesystem fs = GetFilesystem(root);

    // Select scanner: MFT for NTFS if admin available, FindFile otherwise
    IFileSystemScanner* scanner = m_findScanner.get();

    if (fs == DriveFilesystem::NTFS && m_mftScanner->IsMftAvailable(root)) {
        scanner = m_mftScanner.get();
        EmitStatus(IndexerState::Scanning, L"Indexing " + root + L" (MFT mode)...", m_filesIndexed);
    } else if (fs == DriveFilesystem::NTFS) {
        EmitStatus(
            IndexerState::Scanning,
            L"Indexing " + root + L" (standard mode - run as administrator for faster MFT scan)...",
            m_filesIndexed);
    } else {
        EmitStatus(IndexerState::Scanning, L"Indexing " + root + L" (FAT32)...", m_filesIndexed);
    }

    ScanOptions opts;
    opts.rootPaths = {root};
    opts.excludedPaths = m_settings->GetExcludedPaths();

    scanner->Scan(
        opts,
        [this](const FileEntry& fe) {
            m_indexStore->AddEntry(fe);
            ++m_filesIndexed;
        },
        [this, &root](uint64_t count, const std::wstring& /*dir*/) {
            EmitStatus(IndexerState::Scanning,
                       L"Indexing " + root + L"... " + std::to_wstring(count) + L" files", count);
        },
        m_cancel);
}

}  // namespace winindex
