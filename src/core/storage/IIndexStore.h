#pragma once
#include "../indexer/IFileSystemScanner.h"
#include <cstdint>
#include <vector>

namespace winindex {

/// @brief Interface for managing the in-memory file index and its on-disk persistence.
///
/// The store owns the flat array of FileEntry objects and exposes it read-only for
/// search. Write access follows a Begin/Add/End transaction pattern to allow bulk
/// loading. Incremental updates (from the USN journal or ChangeWatcher) use the
/// Apply* methods.
class IIndexStore {
public:
    virtual ~IIndexStore() = default;

    /// @brief Returns true if a valid, non-expired index exists on disk.
    virtual bool IsIndexValid() const = 0;

    /// @brief Loads the index from disk into memory.
    virtual void Load() = 0;

    /// @brief Persists the current in-memory index to disk (CRC-32 validated binary format).
    virtual void Save() = 0;

    /// @brief Begins a bulk-write transaction. Must be paired with EndWrite().
    virtual void BeginWrite() = 0;

    /// @brief Appends @p entry to the store during a bulk-write transaction.
    /// @param entry File entry to add.
    virtual void AddEntry(const FileEntry& entry) = 0;

    /// @brief Commits the bulk-write transaction and makes entries visible to readers.
    virtual void EndWrite() = 0;

    /// @brief Incrementally adds a new file entry (e.g. from a USN Added event).
    /// @param entry The newly created file.
    virtual void ApplyAdd(const FileEntry& entry) = 0;

    /// @brief Incrementally removes an entry by path (e.g. from a USN Removed event).
    /// @param path Full path of the deleted file.
    virtual void ApplyRemove(const std::wstring& path) = 0;

    /// @brief Incrementally renames an entry (e.g. from a USN Renamed event).
    /// @param oldPath Previous full path.
    /// @param newPath New full path.
    virtual void ApplyRename(const std::wstring& oldPath, const std::wstring& newPath) = 0;

    /// @brief Returns the number of entries currently held in the store.
    virtual uint64_t GetEntryCount() const = 0;

    /// @brief Returns a read-only pointer to the flat entry array.
    ///
    /// The pointer is valid until the next write operation.
    virtual const FileEntry* GetEntries() const = 0;

    /// @brief Returns the persisted USN cursor for @p root from the last index save.
    /// @param root Drive root, e.g. L"C:\\".
    virtual uint64_t GetSavedUsn(const std::wstring& root) const = 0;

    /// @brief Stores the USN cursor for @p root so it can be used for delta replay on restart.
    /// @param root Drive root, e.g. L"C:\\".
    /// @param usn  New USN cursor value returned by IUsnJournalMonitor::ReplaySince.
    virtual void SetSavedUsn(const std::wstring& root, uint64_t usn) = 0;
};

}  // namespace winindex
