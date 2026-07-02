#pragma once
#include <gmock/gmock.h>

#include "storage/IIndexStore.h"

namespace winindex {

class MockIndexStore : public IIndexStore {
public:
    MOCK_METHOD(bool, IsIndexValid, (), (const, override));
    MOCK_METHOD(void, Load, (), (override));
    MOCK_METHOD(void, Save, (), (override));
    MOCK_METHOD(void, BeginWrite, (), (override));
    MOCK_METHOD(void, AddEntry, (const FileEntry& e), (override));
    MOCK_METHOD(void, EndWrite, (), (override));
    MOCK_METHOD(void, ApplyAdd, (const FileEntry& e), (override));
    MOCK_METHOD(void, ApplyRemove, (const std::wstring& path), (override));
    MOCK_METHOD(void, RemoveEntriesUnderPath, (const std::wstring& prefix), (override));
    MOCK_METHOD(void, ApplyRename, (const std::wstring& o, const std::wstring& n), (override));
    MOCK_METHOD(uint64_t, GetEntryCount, (), (const, override));
    MOCK_METHOD(uint64_t, GetSavedUsn, (const std::wstring& root), (const, override));
    MOCK_METHOD(void, SetSavedUsn, (const std::wstring& root, uint64_t usn), (override));
    MOCK_METHOD(uint64_t, GetIndexAgeSeconds, (), (const, override));
};

}  // namespace winindex
