#include "IndexPool.h"

#include <algorithm>

namespace winindex {

void IndexPool::Clear() {
    meta.clear();
    nameLowerPool.clear();
    pathPool.clear();
}

void IndexPool::Reserve(size_t capacity) {
    meta.reserve(capacity);
    nameLowerPool.reserve(capacity * 15);  // avg filename ~15 chars
    pathPool.reserve(capacity * 60);       // avg full path ~60 chars
}

void IndexPool::AddEntry(const FileEntry& e) {
    EntryMeta m{};
    m.size = e.size;
    m.lastModified = e.lastModified;
    m.attributes = e.attributes;
    m.deleted = 0;

    // Append full path to pathPool
    m.pathOffset = static_cast<uint32_t>(pathPool.size());
    m.pathLen = static_cast<uint16_t>(e.path.size());
    pathPool.insert(pathPool.end(), e.path.begin(), e.path.end());

    // nameStart: offset within path to the last path component
    size_t slash = e.path.rfind(L'\\');
    m.nameStart = static_cast<uint16_t>(slash != std::wstring::npos ? slash + 1 : 0);

    // Append lowercased filename to nameLowerPool
    m.nameLowerOffset = static_cast<uint32_t>(nameLowerPool.size());
    if (!e.nameLower.empty()) {
        m.nameLowerLen = static_cast<uint16_t>(e.nameLower.size());
        nameLowerPool.insert(nameLowerPool.end(), e.nameLower.begin(), e.nameLower.end());
    } else {
        // Compute from name (or from path tail if name is empty)
        const wchar_t* srcData = e.name.empty() ? e.path.data() + m.nameStart : e.name.data();
        size_t srcLen =
            e.name.empty() ? static_cast<size_t>(m.pathLen - m.nameStart) : e.name.size();
        m.nameLowerLen = static_cast<uint16_t>(srcLen);
        size_t base = nameLowerPool.size();
        nameLowerPool.insert(nameLowerPool.end(), srcData, srcData + srcLen);
        std::transform(nameLowerPool.begin() + static_cast<ptrdiff_t>(base), nameLowerPool.end(),
                       nameLowerPool.begin() + static_cast<ptrdiff_t>(base), ::towlower);
    }

    meta.push_back(m);
}

std::wstring_view IndexPool::GetNameLower(uint32_t idx) const noexcept {
    const auto& m = meta[idx];
    return {nameLowerPool.data() + m.nameLowerOffset, m.nameLowerLen};
}

std::wstring_view IndexPool::GetPath(uint32_t idx) const noexcept {
    const auto& m = meta[idx];
    return {pathPool.data() + m.pathOffset, m.pathLen};
}

std::wstring_view IndexPool::GetName(uint32_t idx) const noexcept {
    const auto& m = meta[idx];
    return {pathPool.data() + m.pathOffset + m.nameStart,
            static_cast<size_t>(m.pathLen - m.nameStart)};
}

}  // namespace winindex
