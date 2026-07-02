#include "Indexer.h"

#include "DriveEnumerator.h"
#include "MftScanner.h"
#include "UsnJournalMonitor.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <iterator>
#include <thread>
#include <utility>

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
        uint64_t ageSeconds = m_indexStore->GetIndexAgeSeconds();
        m_indexStore->Load();
        if (m_indexStore->GetEntryCount() > 0) {
            EmitStatusDone(m_indexStore->GetEntryCount(), m_settings->GetSelectedDrives(),
                           ageSeconds);
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

    struct IndexParams {
        Indexer* self;
    };
    auto* p = new IndexParams{this};
    m_thread = CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* p = static_cast<IndexParams*>(param);
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

    struct PathsParams {
        Indexer* self;
        std::vector<std::wstring> paths;
    };
    auto* p = new PathsParams{this, std::move(paths)};
    m_thread = CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* p = static_cast<PathsParams*>(param);
            p->self->IndexPathsThread(p->paths);
            delete p;
            return 0;
        },
        p, 0, nullptr);
}

void Indexer::IndexPathsThread(const std::vector<std::wstring>& paths) {
    EmitStatus(IndexerState::Scanning, L"Indexing new paths...");
    std::vector<std::wstring> excludedPaths = m_settings->GetExcludedPaths();
    for (const auto& root : paths) {
        if (m_cancel.load(std::memory_order_relaxed))
            break;
        std::vector<FileEntry> localEntries;
        ScanDriveInto(root, excludedPaths, localEntries);
        for (const auto& fe : localEntries) {
            m_indexStore->ApplyAdd(fe);
            ++m_filesIndexed;
        }
    }
    if (!m_cancel.load(std::memory_order_relaxed)) {
        m_indexStore->Save();
        EmitStatusDone(m_indexStore->GetEntryCount(), paths);
        StartWatchersForRoots(paths);
    }
    SetEvent(m_completionEvent);
}

void Indexer::RemovePaths(std::vector<std::wstring> paths) {
    if (IsIndexing())
        return;

    m_cancel.store(false);
    ResetEvent(m_completionEvent);

    struct RemoveParams {
        Indexer* self;
        std::vector<std::wstring> paths;
    };
    auto* p = new RemoveParams{this, std::move(paths)};
    m_thread = CreateThread(
        nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* p = static_cast<RemoveParams*>(param);
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
    EmitStatusDone(m_indexStore->GetEntryCount(), m_settings->GetSelectedDrives());
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
        IndexerStatus s;
        s.state = state;
        s.message = std::move(message);
        s.filesIndexed = filesIndexed;
        s.skippedPaths = skipped;
        m_statusCallback(s);
    }
}

void Indexer::EmitStatusDone(uint64_t filesIndexed, std::vector<std::wstring> locations,
                             uint64_t ageSeconds) {
    if (m_statusCallback) {
        IndexerStatus s;
        s.state = IndexerState::WatchingForChanges;
        s.filesIndexed = filesIndexed;
        s.locations = std::move(locations);
        s.indexAgeSeconds = ageSeconds;
        m_statusCallback(s);
    }
}

void Indexer::IndexingThread() {
    EmitStatus(IndexerState::Scanning, L"Starting index build...");

    auto selectedDrives = m_settings->GetSelectedDrives();
    if (selectedDrives.empty()) {
        // No drives configured yet (e.g., first-run dialog was skipped).
        // Fall back to all local fixed drives so the app works out of the box.
        auto fixed = EnumerateLocalFixedDrives();
        std::transform(fixed.begin(), fixed.end(), std::back_inserter(selectedDrives),
                       [](const DriveInfo& d) { return d.root; });
    }

    // Snapshot settings before spawning threads; Settings isn't thread-safe.
    std::vector<std::wstring> excludedPaths = m_settings->GetExcludedPaths();

    // Scan each drive on its own thread into a per-drive local buffer.
    const size_t n = selectedDrives.size();
    std::vector<std::vector<FileEntry>> perDrive(n);
    std::vector<std::thread> scanThreads;
    scanThreads.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        scanThreads.emplace_back([this, &selectedDrives, &excludedPaths, &perDrive, i]() {
            ScanDriveInto(selectedDrives[i], excludedPaths, perDrive[i]);
        });
    }
    for (auto& t : scanThreads) t.join();

    if (m_cancel.load(std::memory_order_relaxed)) {
        SetEvent(m_completionEvent);
        return;
    }

    m_indexStore->BeginWrite();
    for (const auto& entries : perDrive) {
        for (const auto& fe : entries) {
            m_indexStore->AddEntry(fe);
            ++m_filesIndexed;
        }
    }
    m_indexStore->EndWrite();
    m_indexStore->Save();
    EmitStatusDone(m_filesIndexed, m_settings->GetSelectedDrives());
    StartLiveMonitoring();
    SetEvent(m_completionEvent);
}

bool Indexer::BuildAndApplyAdd(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
        return false;
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    FileEntry fe;
    size_t slash = path.rfind(L'\\');
    fe.name = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
    fe.nameLower = fe.name;
    std::transform(fe.nameLower.begin(), fe.nameLower.end(), fe.nameLower.begin(), ::towlower);
    fe.path = path;
    fe.size = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
    fe.lastModified = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
                      fad.ftLastWriteTime.dwLowDateTime;
    fe.attributes = fad.dwFileAttributes;
    m_indexStore->ApplyAdd(fe);
    return true;
}

void Indexer::ApplyChange(const FileChangeEvent& evt) {
    if (evt.type == FileChangeType::Added) {
        BuildAndApplyAdd(evt.path);
    } else if (evt.type == FileChangeType::Modified) {
        m_indexStore->ApplyRemove(evt.path);
        BuildAndApplyAdd(evt.path);
    } else if (evt.type == FileChangeType::Removed) {
        m_indexStore->ApplyRemove(evt.path);
    } else if (evt.type == FileChangeType::Renamed) {
        if (!evt.oldPath.empty())
            m_indexStore->ApplyRename(evt.oldPath, evt.path);
        else
            BuildAndApplyAdd(evt.path);
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

void Indexer::ScanDriveInto(const std::wstring& root,
                            const std::vector<std::wstring>& excludedPaths,
                            std::vector<FileEntry>& out) {
    DriveFilesystem fs = GetFilesystem(root);

    // Select scanner: MFT for NTFS if admin available, FindFile otherwise.
    IFileSystemScanner* scanner = m_findScanner.get();

    if (fs == DriveFilesystem::NTFS && m_mftScanner->IsMftAvailable(root)) {
        scanner = m_mftScanner.get();
        EmitStatus(IndexerState::Scanning, L"Indexing " + root + L" (MFT mode)...");
    } else if (fs == DriveFilesystem::NTFS) {
        EmitStatus(IndexerState::Scanning,
                   L"Indexing " + root +
                       L" (standard mode - run as administrator for faster MFT scan)...");
    } else {
        EmitStatus(IndexerState::Scanning, L"Indexing " + root + L" (FAT32)...");
    }

    ScanOptions opts;
    opts.rootPaths = {root};
    opts.excludedPaths = excludedPaths;

    scanner->Scan(
        opts, [&out](const FileEntry& fe) { out.push_back(fe); },
        [this, &root](uint64_t count, const std::wstring& /*dir*/) {
            EmitStatus(IndexerState::Scanning,
                       L"Indexing " + root + L"... " + std::to_wstring(count) + L" files", count);
        },
        m_cancel);
}

}  // namespace winindex
