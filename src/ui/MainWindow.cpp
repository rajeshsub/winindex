#include "MainWindow.h"
#include "FirstRunDialog.h"
#include "SettingsDialog.h"
#include "../core/indexer/MftScanner.h"
#include "../core/indexer/FindFileScanner.h"
#include "../core/indexer/UsnJournalMonitor.h"
#include "../core/settings/PathUtils.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace winindex {

bool MainWindow::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.lpszClassName = kClassName;
    wc.hIcon   = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(IDI_WININDEX),
                     IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    wc.hIconSm = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(IDI_WININDEX),
                     IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    return RegisterClassExW(&wc) != 0;
}

HWND MainWindow::Create(HINSTANCE hInst) {
    return CreateWindowExW(0, kClassName, L"winindex",
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
                            nullptr, nullptr, hInst, nullptr);
}

MainWindow::MainWindow(HWND hwnd) : m_hwnd(hwnd) {}

MainWindow::~MainWindow() {
    m_searchCancel.store(true);
    if (m_searchThread.joinable()) m_searchThread.join();
}

void MainWindow::OnCreate() {
    bool portable = IsPortableMode();
    std::wstring exeDir = GetExeDirectory();

    m_settings    = std::make_shared<Settings>(portable, exeDir);
    m_settings->Load();
    m_indexStore  = std::make_shared<IndexStore>(m_settings);
    m_searchEngine= std::make_shared<SearchEngine>();

    auto mftScanner  = std::make_shared<MftScanner>();
    auto findScanner = std::make_shared<FindFileScanner>();
    auto usnMonitor  = std::make_shared<UsnJournalMonitor>();

    m_indexer = std::make_shared<Indexer>(mftScanner, findScanner,
                                           usnMonitor, m_indexStore, m_settings);

    m_indexer->SetStatusCallback([hwnd = m_hwnd](const IndexerStatus& s) {
        auto* copy = new IndexerStatus(s);
        PostMessageW(hwnd, WM_INDEXER_STATUS, 0, reinterpret_cast<LPARAM>(copy));
    });

    InitControls();
    UpdateMenuCheckmarks();

    if (m_settings->IsFirstRun()) {
        FirstRunDialog dlg(m_hwnd, m_settings);
        if (dlg.Show()) {
            m_settings->SetFirstRunComplete();
            m_settings->Save();
        }
    }

    m_indexer->StartIndexing();
}

void MainWindow::InitControls() {
    // Search bar (edit control)
    m_hSearchBar = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, m_hwnd,
        reinterpret_cast<HMENU>(IDC_SEARCHBAR),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(m_hwnd, GWLP_HINSTANCE)),
        nullptr);

    // Apply a larger font to search bar
    HFONT hFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(m_hSearchBar, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    // ListView (virtual, owner-data)
    m_hListView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA |
        LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        0, 0, 0, 0, m_hwnd,
        reinterpret_cast<HMENU>(IDC_LISTVIEW),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(m_hwnd, GWLP_HINSTANCE)),
        nullptr);

    ListView_SetExtendedListViewStyle(m_hListView,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

    // Columns
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;

    col.pszText = const_cast<LPWSTR>(L"Name");  col.cx = 250; col.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(m_hListView, 0, &col);
    col.pszText = const_cast<LPWSTR>(L"Path");  col.cx = 350;
    ListView_InsertColumn(m_hListView, 1, &col);
    col.pszText = const_cast<LPWSTR>(L"Size");  col.cx = 90; col.fmt = LVCFMT_RIGHT;
    ListView_InsertColumn(m_hListView, 2, &col);
    col.pszText = const_cast<LPWSTR>(L"Date Modified"); col.cx = 140; col.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(m_hListView, 3, &col);

    // Status bar
    m_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, m_hwnd,
        reinterpret_cast<HMENU>(IDC_STATUSBAR),
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(m_hwnd, GWLP_HINSTANCE)),
        nullptr);

    SetStatusText(L"Ready.");
}

void MainWindow::OnSize(int cx, int cy) {
    if (!m_hSearchBar || !m_hListView || !m_hStatusBar) return;

    constexpr int kSearchBarHeight = 32;
    constexpr int kSearchBarPadding = 4;

    // Status bar auto-sizes itself
    SendMessageW(m_hStatusBar, WM_SIZE, 0, 0);
    RECT sbRect{};
    GetWindowRect(m_hStatusBar, &sbRect);
    int sbHeight = sbRect.bottom - sbRect.top;

    MoveWindow(m_hSearchBar, kSearchBarPadding, kSearchBarPadding,
               cx - kSearchBarPadding * 2, kSearchBarHeight, TRUE);

    int listTop = kSearchBarHeight + kSearchBarPadding * 2;
    MoveWindow(m_hListView, 0, listTop, cx, cy - listTop - sbHeight, TRUE);
}

void MainWindow::UpdateMenuCheckmarks() {
    HMENU hMenu = GetMenu(m_hwnd);
    if (!hMenu) return;
    auto opts = m_settings->GetSearchOptions();

    auto check = [&](UINT id, bool checked) {
        CheckMenuItem(hMenu, id, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
    };
    check(ID_SEARCH_REGEX,         opts.useRegex);
    check(ID_SEARCH_CASESENSITIVE, opts.caseSensitive);
    check(ID_SEARCH_WHOLEWORD,     opts.wholeWord);
    check(ID_SEARCH_MATCHPATH,     opts.matchPath);
    check(ID_SEARCH_IGNOREDIACS,   opts.ignoreDiacritics);

    // Enable menu items now that we're ready
    HMENU hSearchMenu = GetSubMenu(hMenu, 0);
    for (int i = 0; i < 5; ++i)
        EnableMenuItem(hSearchMenu, i, MF_BYPOSITION | MF_ENABLED);
}

void MainWindow::OnCommand(WORD id) {
    auto opts = m_settings->GetSearchOptions();

    auto toggleOpt = [&](bool& field) {
        field = !field;
        m_settings->SetSearchOptions(opts);
        m_settings->Save();
        UpdateMenuCheckmarks();
        TriggerSearch();
    };

    switch (id) {
        case ID_SEARCH_REGEX:         toggleOpt(opts.useRegex);         break;
        case ID_SEARCH_CASESENSITIVE: toggleOpt(opts.caseSensitive);    break;
        case ID_SEARCH_WHOLEWORD:     toggleOpt(opts.wholeWord);        break;
        case ID_SEARCH_MATCHPATH:     toggleOpt(opts.matchPath);        break;
        case ID_SEARCH_IGNOREDIACS:   toggleOpt(opts.ignoreDiacritics); break;

        case ID_INDEX_REBUILD:
            m_indexer->StartIndexing(true);
            break;

        case ID_INDEX_SETTINGS: {
            SettingsDialog dlg(m_hwnd, m_settings);
            if (dlg.Show()) {
                m_settings->Save();
                UpdateMenuCheckmarks();
            }
            break;
        }

        case ID_HELP_OPENLOG: {
            std::wstring log = m_settings->GetDataDirectory() + L"\\winindex.log";
            ShellExecuteW(m_hwnd, L"open", log.c_str(), nullptr, nullptr, SW_SHOW);
            break;
        }

        case ID_HELP_ABOUT:
            ShowAbout();
            break;

        case ID_CTX_OPEN:       OpenSelectedFile();             break;
        case ID_CTX_OPENDIR:    OpenContainingFolder();         break;
        case ID_CTX_COPYPATH:   CopySelectedPaths(false);       break;
        case ID_CTX_COPYNAME:   CopySelectedPaths(true);        break;
    }
}

void MainWindow::OnSearchChanged() {
    KillTimer(m_hwnd, kSearchTimerId);

    wchar_t buf[1024]{};
    GetWindowTextW(m_hSearchBar, buf, static_cast<int>(std::size(buf)));
    std::wstring query(buf);

    if (query.size() < 2) {
        m_currentResults.clear();
        ListView_SetItemCount(m_hListView, 0);
        SetStatusText(query.empty()
            ? L"Enter a search term to begin."
            : L"Type at least 2 characters to search...");
        return;
    }

    SetTimer(m_hwnd, kSearchTimerId, kDebounceMs, nullptr);
}

void MainWindow::TriggerSearch() {
    wchar_t buf[1024]{};
    GetWindowTextW(m_hSearchBar, buf, static_cast<int>(std::size(buf)));
    std::wstring query(buf);
    if (query.size() >= 2) ExecuteSearch(query);
}

void MainWindow::ExecuteSearch(std::wstring query) {
    m_searchCancel.store(true);
    if (m_searchThread.joinable()) m_searchThread.join();
    m_searchCancel.store(false);

    auto* results    = new std::vector<SearchResult>();
    auto  entries    = m_indexStore->GetEntries();
    auto  count      = m_indexStore->GetEntryCount();
    auto  opts       = m_settings->GetSearchOptions();
    auto  engine     = m_searchEngine;
    auto& cancelRef  = m_searchCancel;
    HWND  hwnd       = m_hwnd;

    m_searchThread = std::thread([=, &cancelRef]() mutable {
        *results = engine->Search(query, entries, count, opts, 10000, cancelRef);
        PostMessageW(hwnd, WM_SEARCH_RESULTS, 0, reinterpret_cast<LPARAM>(results));
    });
}

void MainWindow::OnSearchResults(std::vector<SearchResult>* results) {
    m_totalMatches = results->size(); // simplified; actual total would need separate count pass
    m_currentResults = std::move(*results);
    delete results;

    ListView_SetItemCount(m_hListView, static_cast<int>(m_currentResults.size()));
    InvalidateRect(m_hListView, nullptr, FALSE);

    if (m_currentResults.empty()) {
        SetStatusText(L"No results found.");
    } else {
        std::wstring msg = std::to_wstring(m_currentResults.size()) + L" result(s)";
        if (m_currentResults.size() == 10000)
            msg += L" - showing first 10,000. Refine your search to narrow results.";
        SetStatusText(msg);
    }
}

void MainWindow::OnIndexerStatus(const IndexerStatus& status) {
    // Enable/disable search bar based on whether index is available
    bool hasIndex = (status.state == IndexerState::WatchingForChanges ||
                     status.state == IndexerState::Idle);
    EnableWindow(m_hSearchBar, hasIndex ? TRUE : FALSE);
    if (!hasIndex) {
        ListView_SetItemCount(m_hListView, 0);
    }
    SetStatusText(status.message);
}

void MainWindow::OnListDblClick() {
    int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_currentResults.size())) return;
    const FileEntry* entry = m_currentResults[sel].entry;
    if (!PreCheckFileExists(entry)) return;
    ShellExecuteW(m_hwnd, L"open", entry->path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::OnListKeyDown(NMLVKEYDOWN* kd) {
    if (!kd) return;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch (kd->wVKey) {
        case VK_RETURN:
            if (ctrl) OpenContainingFolder();
            else      OpenSelectedFile();
            break;
        case 'C':
            if (ctrl) CopySelectedPaths(false);
            break;
        case 'X':
            if (ctrl) CutSelectedFiles();
            break;
        case VK_DELETE:
            DeleteSelectedFiles();
            break;
    }
}

void MainWindow::OnContextMenu(HWND hwndFrom, int x, int y) {
    if (hwndFrom != m_hListView) return;
    int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (sel < 0) return;

    HMENU hBase = LoadMenuW(
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(m_hwnd, GWLP_HINSTANCE)),
        MAKEINTRESOURCEW(IDR_CONTEXT_MENU));
    HMENU hPop = GetSubMenu(hBase, 0);

    int selCount = ListView_GetSelectedCount(m_hListView);
    if (selCount > 1) {
        // Disable "Open" for multi-selection — doesn't make sense to open 10 files
        EnableMenuItem(hPop, ID_CTX_OPEN, MF_BYCOMMAND | MF_GRAYED);
    }

    TrackPopupMenu(hPop, TPM_RIGHTBUTTON, x, y, 0, m_hwnd, nullptr);
    DestroyMenu(hBase);
}

bool MainWindow::PreCheckFileExists(const FileEntry* entry) {
    if (GetFileAttributesW(entry->path.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

    std::wstring msg = L"The file no longer exists:\n\n" + entry->path +
                       L"\n\nThe index may be out of date. Would you like to rebuild the index now?";
    int ret = MessageBoxW(m_hwnd, msg.c_str(), L"File Not Found",
                           MB_YESNO | MB_ICONWARNING);
    if (ret == IDYES) m_indexer->StartIndexing(true);
    return false;
}

void MainWindow::OpenSelectedFile() {
    int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_currentResults.size())) return;
    const FileEntry* entry = m_currentResults[sel].entry;
    if (!PreCheckFileExists(entry)) return;
    ShellExecuteW(m_hwnd, L"open", entry->path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::OpenContainingFolder() {
    int sel = ListView_GetNextItem(m_hListView, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_currentResults.size())) return;
    const FileEntry* entry = m_currentResults[sel].entry;

    std::wstring dir = entry->path;
    size_t slash = dir.rfind(L'\\');
    if (slash != std::wstring::npos) dir = dir.substr(0, slash);

    // Open Explorer with file selected
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(entry->path.c_str());
    if (pidl) {
        SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
    }
}

void MainWindow::CopySelectedPaths(bool filenameOnly) {
    std::wstring text;
    int i = -1;
    while ((i = ListView_GetNextItem(m_hListView, i, LVNI_SELECTED)) != -1) {
        if (i >= static_cast<int>(m_currentResults.size())) break;
        const FileEntry* e = m_currentResults[i].entry;
        text += (filenameOnly ? e->name : e->path) + L"\r\n";
    }
    if (text.empty()) return;

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* p = GlobalLock(hMem);
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }
}

void MainWindow::CutSelectedFiles() {
    // Build a DROPFILES structure for shell cut operation
    std::wstring paths;
    int i = -1;
    while ((i = ListView_GetNextItem(m_hListView, i, LVNI_SELECTED)) != -1) {
        if (i >= static_cast<int>(m_currentResults.size())) break;
        paths += m_currentResults[i].entry->path + L'\0';
    }
    if (paths.empty()) return;
    paths += L'\0'; // double null terminate

    size_t dropSize = sizeof(DROPFILES) + (paths.size()) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dropSize);
    if (!hMem) return;

    auto* df = static_cast<DROPFILES*>(GlobalLock(hMem));
    df->pFiles = sizeof(DROPFILES);
    df->fWide  = TRUE;
    memcpy(df + 1, paths.c_str(), paths.size() * sizeof(wchar_t));
    GlobalUnlock(hMem);

    // Set preferred drop effect to Move
    HGLOBAL hEffect = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
    if (hEffect) {
        *static_cast<DWORD*>(GlobalLock(hEffect)) = DROPEFFECT_MOVE;
        GlobalUnlock(hEffect);
    }

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        UINT cfDrop   = CF_HDROP;
        UINT cfEffect = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
        SetClipboardData(cfDrop, hMem);
        if (hEffect) SetClipboardData(cfEffect, hEffect);
        CloseClipboard();
    }
}

void MainWindow::DeleteSelectedFiles() {
    int selCount = ListView_GetSelectedCount(m_hListView);
    if (selCount == 0) return;

    std::wstring msg = L"Are you sure you want to delete " +
                       std::to_wstring(selCount) + L" file(s)?";
    if (MessageBoxW(m_hwnd, msg.c_str(), L"Confirm Delete",
                     MB_YESNO | MB_ICONWARNING) != IDYES) return;

    // Build double-null-terminated path list for SHFileOperation
    std::wstring paths;
    int i = -1;
    while ((i = ListView_GetNextItem(m_hListView, i, LVNI_SELECTED)) != -1) {
        if (i >= static_cast<int>(m_currentResults.size())) break;
        paths += m_currentResults[i].entry->path + L'\0';
    }
    if (paths.empty()) return;
    paths += L'\0';

    SHFILEOPSTRUCTW op{};
    op.hwnd   = m_hwnd;
    op.wFunc  = FO_DELETE;
    op.pFrom  = paths.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    SHFileOperationW(&op);
}

void MainWindow::ShowAbout() {
    MessageBoxW(m_hwnd,
        L"winindex v0.1\n\nBlazingly fast Windows file search.\n\nhttps://github.com/rajeshsub/winindex",
        L"About winindex", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::SetStatusText(const std::wstring& text) {
    SendMessageW(m_hStatusBar, SB_SETTEXTW, 0,
                  reinterpret_cast<LPARAM>(text.c_str()));
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = reinterpret_cast<MainWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* mw = new MainWindow(hwnd);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(mw));
            mw->OnCreate();
            return 0;
        }
        case WM_SIZE:
            if (self) self->OnSize(LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_COMMAND:
            if (HIWORD(wp) == EN_CHANGE && LOWORD(wp) == IDC_SEARCHBAR) {
                if (self) self->OnSearchChanged();
            } else {
                if (self) self->OnCommand(LOWORD(wp));
            }
            return 0;

        case WM_TIMER:
            if (wp == kSearchTimerId && self) {
                KillTimer(hwnd, kSearchTimerId);
                self->TriggerSearch();
            }
            return 0;

        case WM_NOTIFY: {
            auto* hdr = reinterpret_cast<NMHDR*>(lp);
            if (!self) break;
            if (hdr->idFrom == IDC_LISTVIEW) {
                switch (hdr->code) {
                    case NM_DBLCLK:    self->OnListDblClick(); break;
                    case LVN_KEYDOWN:  self->OnListKeyDown(reinterpret_cast<NMLVKEYDOWN*>(lp)); break;
                    case LVN_GETDISPINFOW: {
                        auto* di = reinterpret_cast<NMLVDISPINFOW*>(lp);
                        int idx = di->item.iItem;
                        if (idx < 0 || idx >= static_cast<int>(self->m_currentResults.size()))
                            break;
                        const FileEntry* e = self->m_currentResults[idx].entry;
                        if (di->item.mask & LVIF_TEXT) {
                            switch (di->item.iSubItem) {
                                case 0: di->item.pszText = const_cast<LPWSTR>(e->name.c_str()); break;
                                case 1: {
                                    // Show path without filename
                                    static thread_local std::wstring pathBuf;
                                    size_t slash = e->path.rfind(L'\\');
                                    pathBuf = (slash != std::wstring::npos)
                                              ? e->path.substr(0, slash)
                                              : e->path;
                                    di->item.pszText = const_cast<LPWSTR>(pathBuf.c_str());
                                    break;
                                }
                                case 2: {
                                    static thread_local std::wstring sizeBuf;
                                    if (e->size < 1024)
                                        sizeBuf = std::to_wstring(e->size) + L" B";
                                    else if (e->size < 1024 * 1024)
                                        sizeBuf = std::to_wstring(e->size / 1024) + L" KB";
                                    else
                                        sizeBuf = std::to_wstring(e->size / (1024*1024)) + L" MB";
                                    di->item.pszText = const_cast<LPWSTR>(sizeBuf.c_str());
                                    break;
                                }
                                case 3: {
                                    static thread_local std::wstring dateBuf;
                                    FILETIME ft;
                                    ft.dwLowDateTime  = static_cast<DWORD>(e->lastModified);
                                    ft.dwHighDateTime = static_cast<DWORD>(e->lastModified >> 32);
                                    SYSTEMTIME st{};
                                    FileTimeToLocalFileTime(&ft, &ft);
                                    FileTimeToSystemTime(&ft, &st);
                                    wchar_t buf[64]{};
                                    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d",
                                               st.wYear, st.wMonth, st.wDay,
                                               st.wHour, st.wMinute);
                                    dateBuf = buf;
                                    di->item.pszText = const_cast<LPWSTR>(dateBuf.c_str());
                                    break;
                                }
                            }
                        }
                        break;
                    }
                }
            }
            break;
        }

        case WM_CONTEXTMENU:
            if (self) self->OnContextMenu(reinterpret_cast<HWND>(wp), GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_INDEXER_STATUS: {
            auto* s = reinterpret_cast<IndexerStatus*>(lp);
            if (self && s) self->OnIndexerStatus(*s);
            delete s;
            return 0;
        }

        case WM_SEARCH_RESULTS: {
            auto* r = reinterpret_cast<std::vector<SearchResult>*>(lp);
            if (self && r) self->OnSearchResults(r);
            return 0;
        }

        case WM_DESTROY:
            if (self) {
                delete self;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Needed for WM_CONTEXTMENU GET_X/Y_LPARAM
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

} // namespace winindex
