Status: Accepted

## Context

The in-memory index (millions of `FileEntry` records) must be persisted to disk and
reloaded on startup to avoid re-scanning on every launch.

## Options

| Option | Fits when | Cost now | Extension path | Trade-off |
|--------|-----------|----------|----------------|-----------|
| a. SQLite | Query flexibility needed | FetchContent or system DLL | Full SQL queries | ~5 MB DLL; load time dominated by parsing rows |
| b. JSON / MessagePack | Human-readable or cross-platform | nlohmann/json or msgpack fetch | Schema evolution with versioning | 3-5x larger than binary; slow parse at >1 M records |
| c. Custom binary + CRC-32 | Max load/save speed, single platform | ~300 lines | Versioned header | Not human-readable; schema changes require migration code |

## Decision

Use a **custom binary format** (option c) with a magic number, version field, and CRC-32
integrity check (implemented in `IndexSerializer`).

At 1-5 M records, a flat binary write/read is an order of magnitude faster than any
text or row-oriented format. The app is Windows-only and the format is an internal
cache - cross-platform portability and ad-hoc queryability are not requirements.

## Consequences

- Index loads in < 1 s even for millions of files.
- Format changes require a version bump and migration path (or simply invalidating the
  cache and rebuilding).
- The `.idx` file is not human-inspectable without a dedicated tool.
- CRC-32 detects corruption; on mismatch the index is discarded and rebuilt automatically.
