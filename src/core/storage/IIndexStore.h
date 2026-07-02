#pragma once
#include "../indexer/IFileSystemScanner.h"
#include <cstdint>

namespace winindex {

/// @brief Interface for managing the in-memory file index and its on-disk persistence.
class IIndexStore {
public:
    virtual ~IIndexStore() = default;

    virtual bool IsIndexValid() const = 0;
    virtual void Load() = 0;
    virtual void Save() = 0;

    virtual void BeginWrite() = 0;
    virtual void AddEntry(const FileEntry& entry) = 0;
    virtual void EndWrite() = 0;

    virtual void ApplyAdd(const FileEntry& entry) = 0;
    virtual void ApplyRemove(const std::wstring& path) = 0;
    virtual void RemoveEntriesUnderPath(const std::wstring& prefix) = 0;
    virtual void ApplyRename(const std::wstring& oldPath, const std::wstring& newPath) = 0;

    virtual uint64_t GetEntryCount() const = 0;

    virtual uint64_t GetSavedUsn(const std::wstring& root) const = 0;
    virtual void SetSavedUsn(const std::wstring& root, uint64_t usn) = 0;
    virtual uint64_t GetIndexAgeSeconds() const = 0;
};

}  // namespace winindex
