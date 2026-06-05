#include "IndexSerializer.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace winindex {

// CRC-32 (ISO 3309 polynomial)
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

bool IndexSerializer::Serialize(const std::wstring& filePath, const std::vector<FileEntry>& entries,
                                const std::unordered_map<std::wstring, uint64_t>& usnMap) {
    // Build payload (everything after header) in memory first so we can CRC it
    std::vector<uint8_t> payload;
    payload.reserve(entries.size() * 128);

    auto writeU16 = [&](uint16_t v) {
        payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&v),
                       reinterpret_cast<uint8_t*>(&v) + sizeof(v));
    };
    auto writeU32 = [&](uint32_t v) {
        payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&v),
                       reinterpret_cast<uint8_t*>(&v) + sizeof(v));
    };
    auto writeU64 = [&](uint64_t v) {
        payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&v),
                       reinterpret_cast<uint8_t*>(&v) + sizeof(v));
    };
    auto writeWStr = [&](const std::wstring& s) {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(s.data());
        payload.insert(payload.end(), b, b + s.size() * sizeof(wchar_t));
    };

    for (const auto& e : entries) {
        writeU16(static_cast<uint16_t>(e.name.size()));
        writeU16(static_cast<uint16_t>(e.path.size()));
        writeU64(e.size);
        writeU64(e.lastModified);
        writeU32(e.attributes);
        writeWStr(e.name);
        writeWStr(e.path);
    }

    // USN map
    writeU32(static_cast<uint32_t>(usnMap.size()));
    for (const auto& [root, usn] : usnMap) {
        writeU16(static_cast<uint16_t>(root.size()));
        writeWStr(root);
        writeU64(usn);
    }

    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    uint64_t ts = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;

    IndexFileHeader hdr{};
    hdr.magic = 0x58444957u;
    hdr.version = 1;
    hdr.timestamp = ts;
    hdr.entryCount = static_cast<uint64_t>(entries.size());
    hdr.crc32 = Crc32(payload.data(), payload.size());

    std::ofstream f(filePath, std::ios::binary | std::ios::trunc);
    if (!f)
        return false;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
    return f.good();
}

bool IndexSerializer::Deserialize(const std::wstring& filePath, std::vector<FileEntry>& entries,
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

    if (hdr.magic != 0x58444957u)
        return false;
    if (hdr.version != 1)
        return false;

    size_t payloadSize = fileSize - sizeof(IndexFileHeader);
    std::vector<uint8_t> payload(payloadSize);
    f.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payloadSize));
    if (!f)
        return false;

    if (Crc32(payload.data(), payloadSize) != hdr.crc32)
        return false;

    outTimestamp = hdr.timestamp;
    entries.resize(static_cast<size_t>(hdr.entryCount));

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
    auto readWStr = [&](size_t chars) -> std::wstring {
        std::wstring s(chars, L'\0');
        memcpy(s.data(), p, chars * sizeof(wchar_t));
        p += chars * sizeof(wchar_t);
        return s;
    };

    for (auto& e : entries) {
        if (p + 4 > end)
            return false;
        uint16_t nameLen = readU16();
        uint16_t pathLen = readU16();
        e.size = readU64();
        e.lastModified = readU64();
        e.attributes = readU32();
        e.name = readWStr(nameLen);
        e.nameLower = e.name;
        std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
        e.path = readWStr(pathLen);
    }

    if (p + sizeof(uint32_t) <= end) {
        uint32_t usnCount = readU32();
        for (uint32_t i = 0; i < usnCount && p < end; ++i) {
            uint16_t rootLen = readU16();
            std::wstring root = readWStr(rootLen);
            uint64_t usn = readU64();
            usnMap[root] = usn;
        }
    }

    return true;
}

}  // namespace winindex
