#pragma once
#include "../settings/Settings.h"
#include "IIndexStore.h"
#include "IndexPool.h"
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace winindex {

class IndexStore : public IIndexStore {
public:
    explicit IndexStore(std::shared_ptr<Settings> settings);

    bool IsIndexValid() const override;
    void Load() override;
    void Save() override;

    void BeginWrite() override;
    void AddEntry(const FileEntry& e) override;
    void EndWrite() override;

    void ApplyAdd(const FileEntry& entry) override;
    void ApplyRemove(const std::wstring& path) override;
    void ApplyRename(const std::wstring& oldPath, const std::wstring& newPath) override;
    void RemoveEntriesUnderPath(const std::wstring& prefix) override;

    uint64_t GetEntryCount() const override;

    uint64_t GetSavedUsn(const std::wstring& root) const override;
    void SetSavedUsn(const std::wstring& root, uint64_t usn) override;
    uint64_t GetIndexAgeSeconds() const override;

    // Pool access for search — callers must hold GetSearchMutex() shared lock.
    const IndexPool& GetPool() const noexcept { return m_pool; }
    std::shared_mutex& GetSearchMutex() noexcept { return m_mutex; }

private:
    std::shared_ptr<Settings> m_settings;
    IndexPool m_pool;
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::wstring, uint64_t> m_usnMap;

    // Staging buffer for BeginWrite / AddEntry / EndWrite bulk transactions.
    // Filled without holding m_mutex; swapped in under exclusive lock in EndWrite.
    std::vector<FileEntry> m_stagingBuf;

    std::wstring IndexFilePath() const;
};

}  // namespace winindex
