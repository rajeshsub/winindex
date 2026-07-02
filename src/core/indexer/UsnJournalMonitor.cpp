#include "UsnJournalMonitor.h"

#include <winioctl.h>

#include <vector>

namespace winindex {

HANDLE UsnJournalMonitor::OpenVolume(const std::wstring& root) {
    std::wstring volPath = L"\\\\.\\" + root.substr(0, 2);
    return CreateFileW(volPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                       OPEN_EXISTING, 0, nullptr);
}

bool UsnJournalMonitor::IsAvailable(const std::wstring& root) const {
    HANDLE hVol = OpenVolume(root);
    if (hVol == INVALID_HANDLE_VALUE)
        return false;

    USN_JOURNAL_DATA_V0 jd{};
    DWORD bytes = 0;
    bool ok = DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &jd, sizeof(jd), &bytes,
                              nullptr);
    CloseHandle(hVol);
    return ok;
}

uint64_t UsnJournalMonitor::ReplaySince(const std::wstring& root, uint64_t savedUsn,
                                        ChangeCallback onChange) {
    HANDLE hVol = OpenVolume(root);
    if (hVol == INVALID_HANDLE_VALUE)
        return savedUsn;

    USN_JOURNAL_DATA_V0 jd{};
    DWORD bytes = 0;
    if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &jd, sizeof(jd), &bytes,
                         nullptr)) {
        CloseHandle(hVol);
        return savedUsn;
    }

    READ_USN_JOURNAL_DATA_V0 rjd{};
    rjd.StartUsn = static_cast<USN>(savedUsn);
    rjd.ReasonMask = USN_REASON_FILE_CREATE | USN_REASON_FILE_DELETE | USN_REASON_RENAME_NEW_NAME |
                     USN_REASON_RENAME_OLD_NAME | USN_REASON_DATA_OVERWRITE |
                     USN_REASON_DATA_EXTEND;
    rjd.ReturnOnlyOnClose = FALSE;
    rjd.Timeout = 0;
    rjd.BytesToWaitFor = 0;
    rjd.UsnJournalID = jd.UsnJournalID;

    constexpr DWORD bufSize = 512 * 1024;
    std::vector<BYTE> buf(bufSize);
    USN lastUsn = static_cast<USN>(savedUsn);

    while (true) {
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(hVol, FSCTL_READ_USN_JOURNAL, &rjd, sizeof(rjd), buf.data(), bufSize,
                             &bytesReturned, nullptr)) {
            break;
        }
        if (bytesReturned <= sizeof(USN))
            break;

        lastUsn = *reinterpret_cast<USN*>(buf.data());
        rjd.StartUsn = lastUsn;

        auto* record = reinterpret_cast<USN_RECORD*>(buf.data() + sizeof(USN));
        while (reinterpret_cast<BYTE*>(record) < buf.data() + bytesReturned) {
            std::wstring name(record->FileName, record->FileNameLength / sizeof(wchar_t));

            FileChangeEvent evt;
            evt.path = root + name;  // simplified; full path resolution requires FRN lookup

            if (record->Reason & USN_REASON_FILE_CREATE)
                evt.type = FileChangeType::Added;
            else if (record->Reason & USN_REASON_FILE_DELETE)
                evt.type = FileChangeType::Removed;
            else if (record->Reason & USN_REASON_RENAME_OLD_NAME)
                evt.type = FileChangeType::Removed;
            else if (record->Reason & USN_REASON_RENAME_NEW_NAME)
                evt.type = FileChangeType::Added;
            else
                evt.type = FileChangeType::Modified;

            if (onChange)
                onChange(evt);

            record = reinterpret_cast<USN_RECORD*>(reinterpret_cast<BYTE*>(record) +
                                                   record->RecordLength);
        }
    }

    CloseHandle(hVol);
    return static_cast<uint64_t>(lastUsn);
}

void UsnJournalMonitor::StartMonitoring(const std::wstring& root, uint64_t startUsn,
                                        ChangeCallback onChange,
                                        const std::atomic<bool>& stopToken) {
    HANDLE hVol = OpenVolume(root);
    if (hVol == INVALID_HANDLE_VALUE)
        return;

    USN_JOURNAL_DATA_V0 jd{};
    DWORD bytes = 0;
    if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &jd, sizeof(jd), &bytes,
                         nullptr)) {
        CloseHandle(hVol);
        return;
    }

    READ_USN_JOURNAL_DATA_V0 rjd{};
    rjd.StartUsn = static_cast<USN>(startUsn);
    rjd.ReasonMask = USN_REASON_FILE_CREATE | USN_REASON_FILE_DELETE | USN_REASON_RENAME_NEW_NAME |
                     USN_REASON_RENAME_OLD_NAME | USN_REASON_DATA_OVERWRITE |
                     USN_REASON_DATA_EXTEND;
    rjd.ReturnOnlyOnClose = TRUE;
    rjd.Timeout = 1;  // seconds to wait for new records
    rjd.BytesToWaitFor = 1;
    rjd.UsnJournalID = jd.UsnJournalID;

    constexpr DWORD bufSize = 512 * 1024;
    std::vector<BYTE> buf(bufSize);

    while (!stopToken.load(std::memory_order_relaxed)) {
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(hVol, FSCTL_READ_USN_JOURNAL, &rjd, sizeof(rjd), buf.data(), bufSize,
                             &bytesReturned, nullptr)) {
            break;
        }
        if (bytesReturned <= sizeof(USN))
            continue;

        rjd.StartUsn = *reinterpret_cast<USN*>(buf.data());

        auto* record = reinterpret_cast<USN_RECORD*>(buf.data() + sizeof(USN));
        while (reinterpret_cast<BYTE*>(record) < buf.data() + bytesReturned) {
            std::wstring name(record->FileName, record->FileNameLength / sizeof(wchar_t));

            FileChangeEvent evt;
            evt.path = root + name;

            if (record->Reason & USN_REASON_FILE_CREATE)
                evt.type = FileChangeType::Added;
            else if (record->Reason & USN_REASON_FILE_DELETE)
                evt.type = FileChangeType::Removed;
            else if (record->Reason & USN_REASON_RENAME_OLD_NAME)
                evt.type = FileChangeType::Removed;
            else if (record->Reason & USN_REASON_RENAME_NEW_NAME)
                evt.type = FileChangeType::Added;
            else
                evt.type = FileChangeType::Modified;

            if (onChange)
                onChange(evt);

            record = reinterpret_cast<USN_RECORD*>(reinterpret_cast<BYTE*>(record) +
                                                   record->RecordLength);
        }
    }

    CloseHandle(hVol);
}

}  // namespace winindex
