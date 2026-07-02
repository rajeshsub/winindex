Status: Accepted

## Context

Indexing a full drive requires enumerating every file. The conventional approach
(`FindFirstFile`/`FindNextFile` BFS) is correct but slow on large volumes - several
minutes for a 500 GB NTFS drive with millions of files.

## Options

| Option | Fits when | Cost now | Extension path | Trade-off |
|--------|-----------|----------|----------------|-----------|
| a. FindFirstFile BFS only | Any filesystem, any privilege | Zero | None | 2-10 min for large NTFS volumes |
| b. MFT direct read (NTFS only, admin) | NTFS + elevation available | ~200 lines | Incremental USN replay | 10-100x faster; requires SeManageVolumePrivilege or admin |
| c. WMI / CIM | No native code | COM overhead, slow | Limited | Far slower than FindFirstFile; adds large dependency |

## Decision

**Dual-mode** (option a + b): attempt MFT direct read (`FSCTL_ENUM_USN_DATA`) when the
volume is NTFS and the process has the necessary privileges; fall back to FindFirstFile BFS
otherwise (non-NTFS volumes, non-elevated sessions).

The scanner is abstracted behind `IFileSystemScanner` so the choice is made per-drive at
runtime and the indexer is not coupled to either implementation.

## Consequences

- Elevated sessions index an NTFS drive in seconds; non-elevated sessions still work, just slower.
- Two scanner implementations must be maintained in parallel.
- MFT layout assumptions are NTFS-specific; FAT32/exFAT/ReFS always fall back.
- USN journal replay for incremental updates reuses the MFT privilege model.
