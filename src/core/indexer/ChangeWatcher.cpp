#include "ChangeWatcher.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>

namespace winindex {

ChangeWatcher::ChangeWatcher(std::wstring root) : m_root(std::move(root)) {}

ChangeWatcher::~ChangeWatcher() { Stop(); }

void ChangeWatcher::Start(ChangeCallback onChange) {
    m_stop.store(false);
    m_hStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    m_hDir = CreateFileW(m_root.c_str(), FILE_LIST_DIRECTORY,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

    auto* params       = new ThreadParams{ this, std::move(onChange) };
    m_thread = CreateThread(nullptr, 0, WatchThread, params, 0, nullptr);
}

void ChangeWatcher::Stop() {
    m_stop.store(true);
    if (m_hStop != INVALID_HANDLE_VALUE) SetEvent(m_hStop);
    if (m_thread) {
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }
    if (m_hDir != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDir);
        m_hDir = INVALID_HANDLE_VALUE;
    }
    if (m_hStop != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hStop);
        m_hStop = INVALID_HANDLE_VALUE;
    }
}

DWORD WINAPI ChangeWatcher::WatchThread(LPVOID param) {
    auto* p = static_cast<ThreadParams*>(param);
    p->self->RunWatch(p->onChange);
    delete p;
    return 0;
}

void ChangeWatcher::RunWatch(ChangeCallback& onChange) {
    if (m_hDir == INVALID_HANDLE_VALUE) return;

    constexpr DWORD bufSize = 64 * 1024;
    std::vector<BYTE> buf(bufSize);
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    HANDLE waitHandles[2] = { ov.hEvent, m_hStop };

    while (!m_stop.load(std::memory_order_relaxed)) {
        DWORD bytesReturned = 0;
        ResetEvent(ov.hEvent);

        ReadDirectoryChangesW(m_hDir, buf.data(), bufSize, TRUE,
                              FILE_NOTIFY_CHANGE_FILE_NAME |
                              FILE_NOTIFY_CHANGE_DIR_NAME  |
                              FILE_NOTIFY_CHANGE_SIZE      |
                              FILE_NOTIFY_CHANGE_LAST_WRITE,
                              &bytesReturned, &ov, nullptr);

        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult != WAIT_OBJECT_0) break; // stop event or error

        if (!GetOverlappedResult(m_hDir, &ov, &bytesReturned, FALSE)) break;
        if (bytesReturned == 0) continue; // buffer overflow — changes missed

        auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buf.data());
        while (info) {
            std::wstring name(info->FileName, info->FileNameLength / sizeof(wchar_t));
            std::wstring fullPath = m_root;
            if (fullPath.back() != L'\\') fullPath += L'\\';
            fullPath += name;

            FileChangeEvent evt;
            evt.path = fullPath;
            switch (info->Action) {
                case FILE_ACTION_ADDED:            evt.type = FileChangeType::Added;    break;
                case FILE_ACTION_REMOVED:          evt.type = FileChangeType::Removed;  break;
                case FILE_ACTION_RENAMED_NEW_NAME: evt.type = FileChangeType::Renamed;  break;
                case FILE_ACTION_RENAMED_OLD_NAME: evt.type = FileChangeType::Renamed;  break;
                default:                           evt.type = FileChangeType::Modified; break;
            }
            if (onChange) onChange(evt);

            if (info->NextEntryOffset == 0) break;
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
        }
    }

    CloseHandle(ov.hEvent);
}

} // namespace winindex
