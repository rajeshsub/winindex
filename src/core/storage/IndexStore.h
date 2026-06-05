#pragma once
#include "../settings/Settings.h"
#include "IIndexStore.h"
#include <memory>
#include <mutex>
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

    uint64_t GetEntryCount() const override;
    const FileEntry* GetEntries() const override;

    uint64_t GetSavedUsn(const std::wstring& root) const override;
    void SetSavedUsn(const std::wstring& root, uint64_t usn) override;

private:
    std::shared_ptr<Settings> m_settings;
    std::vector<FileEntry> m_entries;
    mutable std::mutex m_mutex;
    std::unordered_map<std::wstring, uint64_t> m_usnMap;  // root -> last USN

    static constexpr uint32_t MAGIC = 0x58444957;  // "WIDX"
    static constexpr uint16_t VERSION = 1;

    std::wstring IndexFilePath() const;
    uint32_t ComputeCrc32(const void* data, size_t len) const;
};

}  // namespace winindex
