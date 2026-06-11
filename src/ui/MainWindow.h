#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <commctrl.h>

#include "../core/indexer/Indexer.h"
#include "../core/search/ISearchEngine.h"
#include "../core/search/SearchEngine.h"
#include "../core/settings/Settings.h"
#include "../core/storage/IndexStore.h"
#include "resource.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace winindex {

class MainWindow {
public:
    static bool Register(HINSTANCE hInst);
    static HWND Create(HINSTANCE hInst);

    explicit MainWindow(HWND hwnd);
    ~MainWindow();

    void OnCreate();
    void OnSize(int cx, int cy);
    void OnCommand(WORD id);
    void OnContextMenu(HWND hwndFrom, int x, int y);
    void OnSearchChanged();  // called from SearchBar on text change
    void OnListDblClick();
    void OnListKeyDown(const NMLVKEYDOWN* kd);
    void OnIndexerStatus(const IndexerStatus& status);
    void OnSearchResults(std::vector<SearchResult>* results);
    void OnDeviceChange(WPARAM event, LPARAM lp);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK SearchBarSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

private:
    HWND m_hwnd = nullptr;
    HWND m_hSearchBar = nullptr;
    HWND m_hListView = nullptr;
    HWND m_hStatusBar = nullptr;
    HMENU m_hMenu = nullptr;

    std::shared_ptr<Settings> m_settings;
    std::shared_ptr<IndexStore> m_indexStore;
    std::shared_ptr<Indexer> m_indexer;
    std::shared_ptr<SearchEngine> m_searchEngine;

    std::vector<SearchResult> m_currentResults;
    uint64_t m_totalMatches = 0;
    int m_sortColumn = -1;
    bool m_sortDescending = false;

    // Debounce timer
    static constexpr UINT_PTR kSearchTimerId = 1;
    static constexpr UINT kDebounceMs = 150;
    std::atomic<bool> m_searchCancel{false};
    std::thread m_searchThread;
    HDEVNOTIFY m_hDevNotify = nullptr;

    void InitControls();
    void UpdateMenuCheckmarks();
    void TriggerSearch();
    void ExecuteSearch(const std::wstring& query);
    void OpenSelectedFile();
    void OpenContainingFolder();
    void CopySelectedPaths(bool filenameOnly);
    void CutSelectedFiles();
    void DeleteSelectedFiles();
    bool PreCheckFileExists(const FileEntry* entry);
    void ShowAbout();
    void SetStatusText(const std::wstring& text);
    void OnColumnClick(int col);
    void ApplyCurrentSort();

    static constexpr wchar_t kClassName[] = L"WinIndexMainWindow";
};

}  // namespace winindex
