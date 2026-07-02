#include "IndexStore.h"

#include "IndexSerializer.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>

namespace winindex {

IndexStore::IndexStore(std::shared_ptr<Settings> settings) : m_settings(std::move(settings)) {}

std::wstring IndexStore::IndexFilePath() const {
    return m_settings->GetDataDirectory() + L"\\winindex.idx";
}

bool IndexStore::IsIndexValid() const {
    std::wstring path = IndexFilePath();

    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
        return false;

    uint64_t reindexIntervalHours = m_settings->GetReindexIntervalHours();
    if (reindexIntervalHours == 0)
        return true;

    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    uint64_t nowVal = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    uint64_t fileVal = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
                       fad.ftLastWriteTime.dwLowDateTime;

    constexpr uint64_t hundredNsPerHour = 36000000000ULL;
    uint64_t ageHours = (nowVal - fileVal) / hundredNsPerHour;
    return ageHours < reindexIntervalHours;
}

void IndexStore::Load() {
    IndexPool tmp;
    std::unordered_map<std::wstring, uint64_t> usnTmp;
    uint64_t ts = 0;
    bool ok = IndexSerializer::Deserialize(IndexFilePath(), tmp, usnTmp, ts);

    std::unique_lock lock(m_mutex);
    if (ok) {
        m_pool = std::move(tmp);
        m_usnMap = std::move(usnTmp);
    } else {
        m_pool.Clear();
        m_usnMap.clear();
    }
}

void IndexStore::Save() {
    std::shared_lock lock(m_mutex);
    IndexSerializer::Serialize(IndexFilePath(), m_pool, m_usnMap);
}

void IndexStore::BeginWrite() {
    m_stagingBuf.clear();
}

void IndexStore::AddEntry(const FileEntry& e) {
    // Called from single indexing thread — no lock needed on staging buffer.
    m_stagingBuf.push_back(e);
}

void IndexStore::EndWrite() {
    IndexPool fresh;
    fresh.Reserve(m_stagingBuf.size());
    for (const auto& e : m_stagingBuf) fresh.AddEntry(e);
    m_stagingBuf.clear();

    std::unique_lock lock(m_mutex);
    m_pool = std::move(fresh);
}

void IndexStore::ApplyAdd(const FileEntry& entry) {
    std::unique_lock lock(m_mutex);
    m_pool.AddEntry(entry);
}

void IndexStore::ApplyRemove(const std::wstring& path) {
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    std::unique_lock lock(m_mutex);
    for (size_t i = 0; i < m_pool.meta.size(); ++i) {
        if (m_pool.meta[i].deleted)
            continue;
        auto pv = m_pool.GetPath(static_cast<uint32_t>(i));
        std::wstring pl(pv.begin(), pv.end());
        std::transform(pl.begin(), pl.end(), pl.begin(), ::towlower);
        if (pl == lower) {
            m_pool.meta[i].deleted = 1;
            break;
        }
    }
}

void IndexStore::ApplyRename(const std::wstring& oldPath, const std::wstring& newPath) {
    std::wstring oldLower = oldPath;
    std::transform(oldLower.begin(), oldLower.end(), oldLower.begin(), ::towlower);

    std::unique_lock lock(m_mutex);
    for (size_t i = 0; i < m_pool.meta.size(); ++i) {
        if (m_pool.meta[i].deleted)
            continue;
        auto pv = m_pool.GetPath(static_cast<uint32_t>(i));
        std::wstring pl(pv.begin(), pv.end());
        std::transform(pl.begin(), pl.end(), pl.begin(), ::towlower);
        if (pl == oldLower) {
            m_pool.meta[i].deleted = 1;

            // Construct FileEntry for the new path preserving metadata
            const EntryMeta& om = m_pool.meta[i];
            FileEntry fe;
            fe.path = newPath;
            size_t slash = newPath.rfind(L'\\');
            fe.name = (slash != std::wstring::npos) ? newPath.substr(slash + 1) : newPath;
            fe.nameLower = fe.name;
            std::transform(fe.nameLower.begin(), fe.nameLower.end(), fe.nameLower.begin(),
                           ::towlower);
            fe.size = om.size;
            fe.lastModified = om.lastModified;
            fe.attributes = om.attributes;
            m_pool.AddEntry(fe);
            break;
        }
    }
}

void IndexStore::RemoveEntriesUnderPath(const std::wstring& prefix) {
    std::wstring lp = prefix;
    std::transform(lp.begin(), lp.end(), lp.begin(), ::towlower);
    if (!lp.empty() && lp.back() != L'\\')
        lp += L'\\';

    std::unique_lock lock(m_mutex);
    for (size_t i = 0; i < m_pool.meta.size(); ++i) {
        if (m_pool.meta[i].deleted)
            continue;
        auto pv = m_pool.GetPath(static_cast<uint32_t>(i));
        std::wstring pl(pv.begin(), pv.end());
        std::transform(pl.begin(), pl.end(), pl.begin(), ::towlower);
        if (pl.compare(0, lp.size(), lp) == 0)
            m_pool.meta[i].deleted = 1;
    }
}

uint64_t IndexStore::GetEntryCount() const {
    std::shared_lock lock(m_mutex);
    return static_cast<uint64_t>(std::count_if(m_pool.meta.begin(), m_pool.meta.end(),
                                               [](const EntryMeta& m) { return !m.deleted; }));
}

uint64_t IndexStore::GetSavedUsn(const std::wstring& root) const {
    std::shared_lock lock(m_mutex);
    auto it = m_usnMap.find(root);
    return (it != m_usnMap.end()) ? it->second : 0;
}

void IndexStore::SetSavedUsn(const std::wstring& root, uint64_t usn) {
    std::unique_lock lock(m_mutex);
    m_usnMap[root] = usn;
}

uint64_t IndexStore::GetIndexAgeSeconds() const {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(IndexFilePath().c_str(), GetFileExInfoStandard, &fad))
        return UINT64_MAX;
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    uint64_t nowVal = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    uint64_t fileVal = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
                       fad.ftLastWriteTime.dwLowDateTime;
    if (nowVal <= fileVal)
        return 0;
    return (nowVal - fileVal) / 10000000ULL;
}

}  // namespace winindex
