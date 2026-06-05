#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace winindex {

/// @brief Type of change recorded in the NTFS USN journal.
enum class FileChangeType { Added, Removed, Renamed, Modified };

/// @brief A single file-system change event from the USN journal.
struct FileChangeEvent {
    FileChangeType type;        ///< Kind of change.
    std::wstring   path;        ///< Affected file's full path.
    std::wstring   oldPath;     ///< Previous path — only set for Renamed events.
};

/// @brief Callback invoked when a file-system change is detected.
using ChangeCallback = std::function<void(const FileChangeEvent&)>;

/// @brief Interface for replaying and live-monitoring NTFS USN journal changes.
///
/// Enables incremental index updates: replay past changes on startup, then
/// switch to live monitoring so the index stays current without a full rescan.
class IUsnJournalMonitor {
public:
    virtual ~IUsnJournalMonitor() = default;

    /// @brief Returns false if the USN journal is unavailable (FAT32, no admin rights).
    /// @param root Drive root, e.g. L"C:\\".
    virtual bool IsAvailable(const std::wstring& root) const = 0;

    /// @brief Replays all journal records since @p savedUsn, calling @p onChange for each.
    /// @param root      Drive root to query.
    /// @param savedUsn  Last persisted USN; pass 0 to replay from the oldest available record.
    /// @param onChange  Callback invoked for each change.
    /// @return The new USN cursor — persist this so the next startup can call ReplaySince again.
    virtual uint64_t ReplaySince(const std::wstring& root,
                                  uint64_t savedUsn,
                                  ChangeCallback onChange) = 0;

    /// @brief Starts live monitoring, calling @p onChange on the calling thread.
    ///
    /// Intended to be called from a dedicated background thread. Blocks until
    /// @p stopToken is set to true.
    /// @param root       Drive root to monitor.
    /// @param startUsn   USN to begin watching from (typically the value returned by ReplaySince).
    /// @param onChange   Callback invoked for each live change.
    /// @param stopToken  Set to true externally to stop monitoring and return.
    virtual void StartMonitoring(const std::wstring& root,
                                  uint64_t startUsn,
                                  ChangeCallback onChange,
                                  const std::atomic<bool>& stopToken) = 0;
};

} // namespace winindex
