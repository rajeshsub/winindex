#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "IUsnJournalMonitor.h"
#include <atomic>

namespace winindex {

class UsnJournalMonitor : public IUsnJournalMonitor {
public:
    bool IsAvailable(const std::wstring& root) const override;

    uint64_t ReplaySince(const std::wstring& root,
                          uint64_t savedUsn,
                          ChangeCallback onChange) override;

    void StartMonitoring(const std::wstring& root,
                          uint64_t startUsn,
                          ChangeCallback onChange,
                          const std::atomic<bool>& stopToken) override;

private:
    static HANDLE OpenVolume(const std::wstring& root);
};

} // namespace winindex
