# Architecture

Living document. Update when structure changes (rule 19); this describes shape as of 2026-08-22.
For why a decision was made, see `docs/adr/`; this doc describes how the system is now.

## Component map

```
  MftScanner / FindFileScanner   (IFileSystemScanner)
           |
           | Scan(options, onFile, onProgress, cancelToken)
           v
        Indexer  ------------------->  IndexStore (IIndexStore)
     (orchestrates scan/load/watch)         |
           ^                                | owns
           |                                v
  UsnJournalMonitor (IUsnJournalMonitor)  IndexPool (flat wchar_t pools + EntryMeta[])
  ChangeWatcher (ReadDirectoryChangesW)      |
           |                                | GetPool() under shared_mutex
           +-------- Apply* -----------------+
                                              |
                                              v
                                     SearchEngine (ISearchEngine)
                                     RE2 / SIMD substring / token-set
                                              |
                                              | SearchResult[]
                                              v
                                        MainWindow (Win32 UI)
```

## Layout

- `src/core/indexer/` - scanning and change detection.
  - `IFileSystemScanner` - seam over `MftScanner` (FSCTL_ENUM_USN_DATA, needs elevation) and
    `FindFileScanner` (FindFirstFile/FindNextFile BFS, works unelevated). `Indexer` picks one
    per drive at scan time.
  - `IUsnJournalMonitor` / `UsnJournalMonitor` - replays and tails the NTFS USN journal for
    incremental updates without a full rescan.
  - `ChangeWatcher` - `ReadDirectoryChangesW`-based watch for non-NTFS volumes, where the USN
    journal isn't available.
  - `Indexer` - orchestrates the above: decides scan vs. load-from-disk vs. watch, drives
    `IndexerState` (`Idle -> Scanning|LoadingIndex -> WatchingForChanges`, or `Error`), reports
    progress via `StatusCallback`.
- `src/core/storage/` - the index itself.
  - `IndexPool` - the actual data: flat `EntryMeta[]` plus separate contiguous `wchar_t` pools
    for lowercased names and paths, so the hot substring-search path scans a compact working set
    without touching path data.
  - `IndexStore` (`IIndexStore`) - owns `IndexPool` plus a `std::shared_mutex`; mutation
    (`BeginWrite`/`AddEntry`/`EndWrite`, `Apply{Add,Remove,Rename}`) takes an exclusive lock,
    search takes a shared lock via `GetSearchMutex()`. Also owns on-disk persistence
    (`Load`/`Save`, CRC-32 validated `.idx`) and per-drive saved USN cursors.
  - `IndexSerializer` - binary (de)serialization for `IndexStore`'s on-disk format.
- `src/core/search/` - query matching.
  - `ISearchEngine` / `SearchEngine` - dispatches on `SearchOptions`: RE2 (regex mode),
    `SimdSearch`/`SimdSearchAvx2` (substring mode, runtime SIMD dispatch), `TokenMatcher`
    (token-set mode for multi-word/separator queries). Reads `IndexPool` under a caller-held
    shared lock; never mutates it.
- `src/core/settings/` - cross-cutting: `Settings` (INI-backed config, portable-mode aware),
  `PathUtils`, `Logger` (see gap tracked in `docs/engineering-standards/gaps.md`).
- `src/ui/` - Win32 UI. `MainWindow` owns an `Indexer`, an `IndexStore`, and an `ISearchEngine`;
  runs search on a background `std::thread` per keystroke (debounced), copies matched entries
  into UI-owned `DisplayEntry` snapshots (never holds pool references past the lock), and renders
  via a virtual `LVS_OWNERDATA` ListView. `FirstRunDialog` and `SettingsDialog` edit `Settings`.

## Concurrency model

Single writer (`Indexer`/`IndexStore` on the indexing/watch path), multiple readers (`MainWindow`
search threads). Guarded by `IndexStore`'s `std::shared_mutex`: writes exclusive, searches shared.
No network-facing concurrency; not in Service/API scope (rule 22 doesn't apply).

## External dependencies

Fetched via CMake `FetchContent`, pinned by git tag (see `CMakeLists.txt`): GoogleTest (tests
only), Abseil (RE2 dependency), RE2 (regex engine, see `docs/adr/0001-use-re2-for-regex.md`).

## Data flow, end to end

1. First launch: `FirstRunDialog` writes selected drives/exclusions to `Settings`.
2. `Indexer` checks `IndexStore::IsIndexValid()` (CRC-32 + age against
   `ReindexIntervalHours`); loads from disk or triggers a scan per drive via the appropriate
   `IFileSystemScanner`.
3. Scanned `FileEntry` records stream into `IndexStore` (`BeginWrite`/`AddEntry`/`EndWrite`),
   which appends into `IndexPool` and flushes to `winindex.idx` on completion.
4. Post-build, `UsnJournalMonitor` (NTFS) or `ChangeWatcher` (non-NTFS) feed live changes back
   into `IndexStore` via `Apply{Add,Remove,Rename}`.
5. Each keystroke in `MainWindow` spawns a background thread that takes `IndexStore`'s shared
   lock and calls `ISearchEngine::Search` against `IndexPool`, then copies results into
   `DisplayEntry` for rendering.
