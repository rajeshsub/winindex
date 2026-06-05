# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2025-01-01

### Added
- MFT scanner for NTFS drives (FSCTL_ENUM_USN_DATA)
- Fallback FindFirstFile BFS scanner for FAT32 / non-elevated NTFS
- SIMD-accelerated substring search (AVX2, SSE4.2, scalar fallback)
- RE2 regex search engine integration
- Search modes: case-sensitive, whole-word, match-path, ignore-diacritics
- USN journal monitor for live change detection (NTFS)
- ReadDirectoryChangesW watcher for non-NTFS volumes
- IndexStore with binary serialization and CRC-32 validation
- Persistent index loaded on startup; rebuilt when stale or missing
- Win32 UI: search bar with 150 ms debounce, virtual ListView (LVS_OWNERDATA)
- Context menu: open, open folder, copy path, copy name, cut, delete to Recycle Bin
- First-run dialog for drive and exclusion path selection
- Settings dialog and INI-based configuration (portable mode supported)
- Smart default exclusions (Windows, Program Files, AppData, etc.)
- CMake FetchContent for RE2, Abseil, GoogleTest — no manual install needed
- GitHub Actions CI: build and test on every push; ZIP + NSIS release on tags

[Unreleased]: https://github.com/rajeshsub/winindex/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/rajeshsub/winindex/releases/tag/v0.1.0
