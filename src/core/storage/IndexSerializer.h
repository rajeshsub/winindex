#pragma once
#include "IndexPool.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace winindex {

// Binary index file layout (version 2):
//
// Header (26 bytes, #pragma pack(1)):
//   u32 magic        = 0x58444957 ("WIDX")
//   u16 version      = 2
//   u64 timestamp    (FILETIME of index build)
//   u64 entryCount
//   u32 crc32        (of everything after the header)
//
// Payload:
//   u64 pathPoolSize  (char count)
//   wchar_t pathPool[pathPoolSize]      (UTF-16 LE, no null terminators)
//
//   Per-entry disk record (entryCount records, 24 bytes each):
//     u64 size
//     u64 lastModified
//     u32 attributes
//     u16 pathLen
//     u16 nameStart
//
//   USN map:
//     u32 usnEntryCount
//     per entry: u16 rootLen + wchar_t root[rootLen] + u64 usn
//
// nameLower is NOT stored on disk; it is rebuilt from path+nameStart at load time.
// Version 1 files are detected and trigger a silent re-index.

#pragma pack(push, 1)
struct IndexFileHeader {
    uint32_t magic;
    uint16_t version;
    uint64_t timestamp;
    uint64_t entryCount;
    uint32_t crc32;
};
#pragma pack(pop)

class IndexSerializer {
public:
    static bool Serialize(const std::wstring& filePath, const IndexPool& pool,
                          const std::unordered_map<std::wstring, uint64_t>& usnMap);

    static bool Deserialize(const std::wstring& filePath, IndexPool& pool,
                            std::unordered_map<std::wstring, uint64_t>& usnMap,
                            uint64_t& outTimestamp);

private:
    static uint32_t Crc32(const void* data, size_t len);
};

}  // namespace winindex
