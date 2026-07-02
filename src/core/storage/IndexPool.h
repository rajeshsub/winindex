#pragma once
#include "../indexer/IFileSystemScanner.h"
#include <cstdint>
#include <string>
#include <vector>

namespace winindex {

// Fixed-size metadata record for one file entry.
// Offsets address into the flat wchar_t pools in IndexPool.
// sizeof = 40 bytes (36 + 4 trailing alignment pad from uint64_t members).
struct EntryMeta {
    uint64_t size;
    uint64_t lastModified;
    uint32_t pathOffset;       // char offset into IndexPool::pathPool
    uint32_t nameLowerOffset;  // char offset into IndexPool::nameLowerPool
    uint32_t attributes;
    uint16_t pathLen;       // char count of full path
    uint16_t nameLowerLen;  // char count of lowercased filename
    uint16_t nameStart;     // chars from pathOffset where filename begins
    uint8_t deleted;        // non-zero = tombstoned by ApplyRemove / ApplyRename
    uint8_t _pad{};
};

// Flat contiguous string pool for the entire file index.
//
// Separate pools for nameLower and path allow name-only search (the default)
// to scan a compact ~18 MB working set without touching path data, keeping
// the hot scan in L3 cache on modern hardware.
class IndexPool {
public:
    std::vector<EntryMeta> meta;
    std::vector<wchar_t> nameLowerPool;
    std::vector<wchar_t> pathPool;

    void Clear();
    void Reserve(size_t capacity);

    // Appends one entry. name/nameLower in e must be set by caller;
    // nameLower may be empty, in which case it is computed from name.
    void AddEntry(const FileEntry& e);

    uint64_t Size() const noexcept { return meta.size(); }

    // Zero-copy views into pool memory. Valid until the next mutation.
    std::wstring_view GetNameLower(uint32_t idx) const noexcept;
    std::wstring_view GetPath(uint32_t idx) const noexcept;
    std::wstring_view GetName(uint32_t idx) const noexcept;  // tail of path after last backslash
};

}  // namespace winindex
