#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "IUsnJournalMonitor.h"
#include <atomic>
#include <functional>
#include <string>

namespace winindex {

// Watches a non-NTFS root for changes using ReadDirectoryChangesW.
class ChangeWatcher {
public:
    explicit ChangeWatcher(std::wstring root);
    ~ChangeWatcher();

    void Start(ChangeCallback onChange);
    void Stop();

private:
    std::wstring m_root;
    HANDLE m_hDir = INVALID_HANDLE_VALUE;
    HANDLE m_hStop = INVALID_HANDLE_VALUE;
    std::atomic<bool> m_stop{false};
    HANDLE m_thread = nullptr;

    struct ThreadParams {
        ChangeWatcher* self;
        ChangeCallback onChange;
    };

    static DWORD WINAPI WatchThread(LPVOID param);
    void RunWatch(const ChangeCallback& onChange);
};

}  // namespace winindex
