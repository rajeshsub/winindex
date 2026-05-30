#pragma once
#include <gmock/gmock.h>
#include "indexer/IUsnJournalMonitor.h"

namespace winindex {

class MockUsnJournalMonitor : public IUsnJournalMonitor {
public:
    MOCK_METHOD(bool, IsAvailable, (const std::wstring& root), (const, override));
    MOCK_METHOD(uint64_t, ReplaySince,
                (const std::wstring& root, uint64_t savedUsn, ChangeCallback onChange),
                (override));
    MOCK_METHOD(void, StartMonitoring,
                (const std::wstring& root, uint64_t startUsn,
                 ChangeCallback onChange, const std::atomic<bool>& stopToken),
                (override));
};

} // namespace winindex
