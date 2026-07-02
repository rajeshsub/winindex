Status: Accepted

## Context

Profiling on a 624 K-entry index showed two root causes of slow search:

1. **Cache thrashing from heap-scattered strings.** Each `FileEntry` owns three
   heap-allocated `std::wstring` objects (`name`, `nameLower`, `path`). Searching
   624 K entries requires chasing ~1.9 M scattered heap pointers, producing near-zero
   L2/L3 cache reuse and dominating search latency.

2. **No early exit across parallel search threads.** Threads continue scanning after
   `maxResults` hits are found collectively, wasting CPU time on every keystroke.

A third issue — `GetEntries()` returning a raw pointer with no lock while ChangeWatcher
threads can concurrently call `ApplyAdd()` and reallocate the vector — is an active data
race. See also ADR-0003 (binary format) which this supersedes for the in-memory layout.

## Options

| Option | Fits when | Cost now | Extension path | Trade-off |
|--------|-----------|----------|----------------|-----------|
| a. Keep `vector<FileEntry>` with per-entry `wstring` | Index is small (<100 K entries) | None | None | Cache-hostile at 600 K+ entries; scales poorly |
| b. Flat pool: single `vector<wchar_t>` with offsets | Sequential scan is the primary access pattern | Medium refactor | Add sorted/trigram index later | O(n) scan but L3-resident; correct for 600 K scale |
| c. Sorted array + binary search | Prefix-match queries dominate | High refactor | Replace with trie later | Only fast for prefix queries; substring still O(n) |
| d. Trigram inverted index | Arbitrary substring at >10 M entries | High complexity | Standard IR approach | Overkill at 600 K; large memory overhead |

## Decision

Adopt **option b** — separate flat string pools:

- **`nameLower` pool** (`vector<wchar_t>`): all lowercased filenames concatenated,
  ~18 MB for 624 K entries. Fits in L3 cache. This is the default search target.
- **`path` pool** (`vector<wchar_t>`): all full paths original-case concatenated,
  ~75 MB. Used only when `matchPath = true` (opt-in).
- **Metadata array** (`vector<EntryMeta>`): fixed-size structs (32 bytes each) holding
  offsets and lengths into both pools, plus `size`, `lastModified`, `attributes`.

`SearchResult::entry` changes from `const FileEntry*` to `uint32_t entryIndex`. The UI
looks up display fields via a thin `IndexPool::GetEntry(index)` call.

Concurrency is managed by `std::shared_mutex` (backed by Windows SRWLOCK — no heavier
than a CRITICAL_SECTION). Search acquires a shared read lock for the duration of the
scan (~5-10 ms). ChangeWatcher acquires an exclusive write lock to append entries.

A `pathLower` pool is deferred: path search (`matchPath = true`) is opt-in and not the
latency-sensitive default. It can be added as a fourth pool if path-search performance
becomes a concern.

On-disk format is bumped to **version 2** (see ADR-0003). Version mismatch triggers a
one-time silent re-index on first launch after the upgrade.

## Consequences

- Name-only search scans ~18 MB of contiguous memory instead of chasing ~1.9 M heap
  pointers — expected 3-5x throughput improvement.
- Data race on `GetEntries()` is eliminated by `shared_mutex`.
- `SearchResult` is a breaking API change: callers must use `entryIndex` + pool lookup
  rather than a `FileEntry*`.
- `nameLower` is not persisted on disk; it is recomputed from `name` at load time
  (one-time cost at startup, O(n) `towlower` pass).
- `pathLower` is not stored; path search uses a per-thread lowercase buffer as before.
- On-disk format version 2 is incompatible with version 1; existing `.idx` files are
  discarded and rebuilt on first launch.
