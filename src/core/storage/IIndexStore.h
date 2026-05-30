#pragma once
#include "../indexer/IFileSystemScanner.h"
#include <vector>
#include <cstdint>

namespace winindex {

class IIndexStore {
public:
    virtual ~IIndexStore() = default;

    // Returns true if a valid, non-expired index exists on disk.
    virtual bool IsIndexValid() const = 0;

    virtual void Load()  = 0;
    virtual void Save()  = 0;

    virtual void BeginWrite() = 0;
    virtual void AddEntry(const FileEntry& entry) = 0;
    virtual void EndWrite() = 0;

    // Incremental updates from live monitoring
    virtual void ApplyAdd(const FileEntry& entry)         = 0;
    virtual void ApplyRemove(const std::wstring& path)    = 0;
    virtual void ApplyRename(const std::wstring& oldPath,
                              const std::wstring& newPath) = 0;

    virtual uint64_t              GetEntryCount() const         = 0;
    virtual const FileEntry*      GetEntries()    const         = 0;

    // Per-drive saved USN for NTFS delta replay on restart
    virtual uint64_t GetSavedUsn(const std::wstring& root) const = 0;
    virtual void     SetSavedUsn(const std::wstring& root, uint64_t usn) = 0;
};

} // namespace winindex
