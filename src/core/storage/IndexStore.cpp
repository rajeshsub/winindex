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

    // Check age against configured reindex interval
    uint64_t reindexIntervalHours = m_settings->GetReindexIntervalHours();
    if (reindexIntervalHours == 0)
        return true;  // Manual only — always treat as valid

    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    uint64_t nowVal = (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    uint64_t fileVal = (static_cast<uint64_t>(fad.ftLastWriteTime.dwHighDateTime) << 32) |
                       fad.ftLastWriteTime.dwLowDateTime;

    // FILETIME is in 100-nanosecond intervals
    constexpr uint64_t hundredNsPerHour = 36000000000ULL;
    uint64_t ageHours = (nowVal - fileVal) / hundredNsPerHour;
    return ageHours < reindexIntervalHours;
}

void IndexStore::Load() {
    std::lock_guard lock(m_mutex);
    m_entries.clear();
    m_usnMap.clear();
    uint64_t ts = 0;
    if (!IndexSerializer::Deserialize(IndexFilePath(), m_entries, m_usnMap, ts)) {
        m_entries.clear();
        m_usnMap.clear();
    }
}

void IndexStore::Save() {
    std::lock_guard lock(m_mutex);
    IndexSerializer::Serialize(IndexFilePath(), m_entries, m_usnMap);
}

void IndexStore::BeginWrite() {
    std::lock_guard lock(m_mutex);
    m_entries.clear();
}

void IndexStore::AddEntry(const FileEntry& e) {
    std::lock_guard lock(m_mutex);
    m_entries.push_back(e);
}

void IndexStore::EndWrite() {
    // Nothing to flush — entries are in memory until Save()
}

void IndexStore::ApplyAdd(const FileEntry& entry) {
    std::lock_guard lock(m_mutex);
    m_entries.push_back(entry);
}

void IndexStore::ApplyRemove(const std::wstring& path) {
    std::lock_guard lock(m_mutex);
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&](const FileEntry& e) {
                                       std::wstring p = e.path;
                                       std::transform(p.begin(), p.end(), p.begin(), ::towlower);
                                       return p == lower;
                                   }),
                    m_entries.end());
}

void IndexStore::RemoveEntriesUnderPath(const std::wstring& prefix) {
    std::wstring lp = prefix;
    std::transform(lp.begin(), lp.end(), lp.begin(), ::towlower);
    if (!lp.empty() && lp.back() != L'\\')
        lp += L'\\';

    std::lock_guard lock(m_mutex);
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&](const FileEntry& e) {
                                       std::wstring ep = e.path;
                                       std::transform(ep.begin(), ep.end(), ep.begin(), ::towlower);
                                       return ep.compare(0, lp.size(), lp) == 0;
                                   }),
                    m_entries.end());
}

void IndexStore::ApplyRename(const std::wstring& oldPath, const std::wstring& newPath) {
    std::lock_guard lock(m_mutex);
    std::wstring oldLower = oldPath;
    std::transform(oldLower.begin(), oldLower.end(), oldLower.begin(), ::towlower);
    for (auto& e : m_entries) {
        std::wstring p = e.path;
        std::transform(p.begin(), p.end(), p.begin(), ::towlower);
        if (p == oldLower) {
            e.path = newPath;
            size_t slash = newPath.rfind(L'\\');
            e.name = (slash != std::wstring::npos) ? newPath.substr(slash + 1) : newPath;
            e.nameLower = e.name;
            std::transform(e.nameLower.begin(), e.nameLower.end(), e.nameLower.begin(), ::towlower);
            break;
        }
    }
}

uint64_t IndexStore::GetEntryCount() const {
    std::lock_guard lock(m_mutex);
    return m_entries.size();
}

const FileEntry* IndexStore::GetEntries() const {
    return m_entries.data();
}

uint64_t IndexStore::GetSavedUsn(const std::wstring& root) const {
    std::lock_guard lock(m_mutex);
    auto it = m_usnMap.find(root);
    return (it != m_usnMap.end()) ? it->second : 0;
}

void IndexStore::SetSavedUsn(const std::wstring& root, uint64_t usn) {
    std::lock_guard lock(m_mutex);
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
