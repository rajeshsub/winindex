#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace winindex {

enum class FileChangeType { Added, Removed, Renamed, Modified };

struct FileChangeEvent {
    FileChangeType type;
    std::wstring   path;
    std::wstring   oldPath; // only set for Renamed
};

using ChangeCallback = std::function<void(const FileChangeEvent&)>;

class IUsnJournalMonitor {
public:
    virtual ~IUsnJournalMonitor() = default;

    // Returns false if USN journal not available (e.g. FAT32, no admin).
    virtual bool IsAvailable(const std::wstring& root) const = 0;

    // Replay changes since savedUsn. Returns the new USN to persist.
    virtual uint64_t ReplaySince(const std::wstring& root,
                                  uint64_t savedUsn,
                                  ChangeCallback onChange) = 0;

    // Start live monitoring. Calls onChange on the calling thread (run on background thread).
    virtual void StartMonitoring(const std::wstring& root,
                                  uint64_t startUsn,
                                  ChangeCallback onChange,
                                  const std::atomic<bool>& stopToken) = 0;
};

} // namespace winindex
