#pragma once
#include <gmock/gmock.h>
#include "indexer/IFileSystemScanner.h"

namespace winindex {

class MockFileSystemScanner : public IFileSystemScanner {
public:
    MOCK_METHOD(bool, IsMftAvailable, (const std::wstring& root), (const, override));
    MOCK_METHOD(void, Scan,
                (const ScanOptions& options,
                 ScanCallback onFile,
                 ProgressCallback onProgress,
                 const std::atomic<bool>& cancelToken),
                (override));
};

} // namespace winindex
