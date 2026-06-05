#pragma once
#include "../indexer/IFileSystemScanner.h"
#include <cstdint>
#include <string>
#include <vector>

namespace winindex {

// Binary index file layout:
//
// Header (fixed size):
//   u32 magic        = 0x58444957 ("WIDX")
//   u16 version
//   u64 timestamp    (FILETIME of index build)
//   u64 entryCount
//   u32 crc32        (of everything after the header)
//
// Per entry (variable length, packed):
//   u16 nameLen      (chars, not bytes)
//   u16 pathLen
//   u64 size
//   u64 lastModified
//   u32 attributes
//   wchar_t name[nameLen]
//   wchar_t path[pathLen]
//
// USN map (after entries):
//   u32 usnEntryCount
//   per usn entry:
//     u16 rootLen
//     wchar_t root[rootLen]
//     u64 usn

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
    static bool Serialize(const std::wstring& filePath, const std::vector<FileEntry>& entries,
                          const std::unordered_map<std::wstring, uint64_t>& usnMap);

    static bool Deserialize(const std::wstring& filePath, std::vector<FileEntry>& entries,
                            std::unordered_map<std::wstring, uint64_t>& usnMap,
                            uint64_t& outTimestamp);

private:
    static uint32_t Crc32(const void* data, size_t len);
};

}  // namespace winindex
