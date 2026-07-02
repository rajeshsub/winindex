#include "IndexSerializer.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace winindex {

static constexpr uint32_t kMagic = 0x58444957u;  // "WIDX"
static constexpr uint16_t kVersion = 2;

// Per-entry record as stored on disk (24 bytes, naturally aligned).
#pragma pack(push, 1)
struct DiskEntry {
    uint64_t size;
    uint64_t lastModified;
    uint32_t attributes;
    uint16_t pathLen;
    uint16_t nameStart;
};
#pragma pack(pop)

uint32_t IndexSerializer::Crc32(const void* data, size_t len) {
    static uint32_t table[256] = {};
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

bool IndexSerializer::Serialize(const std::wstring& filePath, const IndexPool& pool,
                                const std::unordered_map<std::wstring, uint64_t>& usnMap) {
    // Build payload in memory so we can CRC it before writing.
    std::vector<uint8_t> payload;

    auto writeRaw = [&](const void* src, size_t bytes) {
        const auto* b = static_cast<const uint8_t*>(src);
        payload.insert(payload.end(), b, b + bytes);
    };
    auto writeU16 = [&](uint16_t v) { writeRaw(&v, sizeof(v)); };
    auto writeU32 = [&](uint32_t v) { writeRaw(&v, sizeof(v)); };
    auto writeU64 = [&](uint64_t v) { writeRaw(&v, sizeof(v)); };

    // Path pool
    uint64_t pathPoolSize = pool.pathPool.size();
    writeU64(pathPoolSize);
    if (pathPoolSize > 0)
        writeRaw(pool.pathPool.data(), pathPoolSize * sizeof(wchar_t));

    // Per-entry disk records (only non-deleted entries)
    // We'll need to recount, so build in two passes: count first, then fill.
    // Actually we write all (deleted bit is not persisted; deleted entries are dropped on Save).
    uint64_t liveCount = 0;
    for (size_t i = 0; i < pool.meta.size(); ++i)
        if (!pool.meta[i].deleted)
            ++liveCount;

    for (size_t i = 0; i < pool.meta.size(); ++i) {
        const EntryMeta& m = pool.meta[i];
        if (m.deleted)
            continue;
        DiskEntry de{};
        de.size = m.size;
        de.lastModified = m.lastModified;
        de.attributes = m.attributes;
        de.pathLen = m.pathLen;
        de.nameStart = m.nameStart;
        writeRaw(&de, sizeof(de));
    }

    // USN map
    writeU32(static_cast<uint32_t>(usnMap.size()));
    for (const auto& [root, usn] : usnMap) {
        writeU16(static_cast<uint16_t>(root.size()));
        writeRaw(root.data(), root.size() * sizeof(wchar_t));
        writeU64(usn);
    }

    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    uint64_t ts = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;

    IndexFileHeader hdr{};
    hdr.magic = kMagic;
    hdr.version = kVersion;
    hdr.timestamp = ts;
    hdr.entryCount = liveCount;
    hdr.crc32 = Crc32(payload.data(), payload.size());

    std::ofstream f(filePath, std::ios::binary | std::ios::trunc);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
    return f.good();
}

bool IndexSerializer::Deserialize(const std::wstring& filePath, IndexPool& pool,
                                  std::unordered_map<std::wstring, uint64_t>& usnMap,
                                  uint64_t& outTimestamp) {
    std::ifstream f(filePath, std::ios::binary | std::ios::ate);
    if (!f)
        return false;

    auto fileSize = static_cast<size_t>(f.tellg());
    if (fileSize < sizeof(IndexFileHeader))
        return false;

    f.seekg(0);
    IndexFileHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));

    if (hdr.magic != kMagic)
        return false;
    if (hdr.version != kVersion)
        return false;  // Version 1 file: caller triggers re-index

    size_t payloadSize = fileSize - sizeof(IndexFileHeader);
    std::vector<uint8_t> payload(payloadSize);
    f.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payloadSize));
    if (!f)
        return false;

    if (Crc32(payload.data(), payloadSize) != hdr.crc32)
        return false;

    outTimestamp = hdr.timestamp;
    pool.Clear();

    const uint8_t* p = payload.data();
    const uint8_t* end = payload.data() + payloadSize;

    auto readU16 = [&]() -> uint16_t {
        uint16_t v{};
        memcpy(&v, p, sizeof(v));
        p += sizeof(v);
        return v;
    };
    auto readU32 = [&]() -> uint32_t {
        uint32_t v{};
        memcpy(&v, p, sizeof(v));
        p += sizeof(v);
        return v;
    };
    auto readU64 = [&]() -> uint64_t {
        uint64_t v{};
        memcpy(&v, p, sizeof(v));
        p += sizeof(v);
        return v;
    };

    // Path pool
    if (p + sizeof(uint64_t) > end)
        return false;
    uint64_t pathPoolSize = readU64();
    size_t pathPoolBytes = static_cast<size_t>(pathPoolSize) * sizeof(wchar_t);
    if (p + pathPoolBytes > end)
        return false;

    pool.pathPool.resize(static_cast<size_t>(pathPoolSize));
    if (pathPoolSize > 0)
        memcpy(pool.pathPool.data(), p, pathPoolBytes);
    p += pathPoolBytes;

    // Per-entry records: reconstruct metadata + nameLower pool
    uint64_t entryCount = hdr.entryCount;
    pool.meta.reserve(static_cast<size_t>(entryCount));
    pool.nameLowerPool.reserve(static_cast<size_t>(entryCount) * 15);

    size_t diskEntrySize = sizeof(DiskEntry);
    if (p + diskEntrySize * entryCount > end)
        return false;

    for (uint64_t i = 0; i < entryCount; ++i) {
        DiskEntry de{};
        memcpy(&de, p, diskEntrySize);
        p += diskEntrySize;

        EntryMeta m{};
        m.size = de.size;
        m.lastModified = de.lastModified;
        m.attributes = de.attributes;
        m.pathLen = de.pathLen;
        m.nameStart = de.nameStart;
        m.deleted = 0;

        // pathOffset: we need to figure out where this entry's path sits in the pool.
        // Since we read pathPool as a whole flat buffer and DiskEntry doesn't store pathOffset
        // (we need to recompute it), we track the running offset using pathLen values.
        // This is done below after the loop.
        // For now, store a sentinel — we'll fix it up.
        m.pathOffset = 0;  // fixed up below

        // Build nameLower from the path tail
        m.nameLowerOffset = static_cast<uint32_t>(pool.nameLowerPool.size());
        uint16_t nameLen = static_cast<uint16_t>(de.pathLen - de.nameStart);
        m.nameLowerLen = nameLen;
        // pathOffset not yet known; use running sum from previous entries
        // We'll fix pathOffset in a second pass below.
        pool.meta.push_back(m);
        pool.nameLowerPool.resize(pool.nameLowerPool.size() + nameLen);
    }

    // Second pass: assign pathOffsets and fill nameLowerPool.
    uint32_t runningPathOffset = 0;
    size_t runningNlOffset = 0;
    for (uint64_t i = 0; i < entryCount; ++i) {
        EntryMeta& m = pool.meta[i];
        m.pathOffset = runningPathOffset;
        m.nameLowerOffset = static_cast<uint32_t>(runningNlOffset);

        // Fill nameLower: lowercase the filename portion of path
        const wchar_t* nameSrc = pool.pathPool.data() + runningPathOffset + m.nameStart;
        wchar_t* nlDst = pool.nameLowerPool.data() + runningNlOffset;
        uint16_t nameLen = m.nameLowerLen;
        for (uint16_t c = 0; c < nameLen; ++c)
            nlDst[c] = static_cast<wchar_t>(::towlower(nameSrc[c]));

        runningPathOffset += m.pathLen;
        runningNlOffset += nameLen;
    }

    // USN map
    if (p + sizeof(uint32_t) <= end) {
        uint32_t usnCount = readU32();
        for (uint32_t i = 0; i < usnCount; ++i) {
            if (p + sizeof(uint16_t) > end)
                break;
            uint16_t rootLen = readU16();
            if (p + rootLen * sizeof(wchar_t) + sizeof(uint64_t) > end)
                break;
            std::wstring root(reinterpret_cast<const wchar_t*>(p), rootLen);
            p += rootLen * sizeof(wchar_t);
            uint64_t usn = readU64();
            usnMap[root] = usn;
        }
    }

    return true;
}

}  // namespace winindex
