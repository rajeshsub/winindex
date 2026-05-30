#include "SettingsDialog.h"
#include "resource.h"
#include <commctrl.h>
#include <shlobj.h>
#include <algorithm>

namespace winindex {

SettingsDialog::SettingsDialog(HWND hParent, std::shared_ptr<Settings> settings)
    : m_hParent(hParent), m_settings(std::move(settings)) {
    m_drives = EnumerateLocalFixedDrives();
}

bool SettingsDialog::Show() {
    INT_PTR result = DialogBoxParamW(
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(m_hParent, GWLP_HINSTANCE)),
        MAKEINTRESOURCEW(IDD_SETTINGS),
        m_hParent,
        DlgProc,
        reinterpret_cast<LPARAM>(this));
    return result == IDOK;
}

INT_PTR CALLBACK SettingsDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SettingsDialog* self = reinterpret_cast<SettingsDialog*>(
        GetWindowLongPtrW(hwnd, DWLP_USER));

    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(hwnd, DWLP_USER, lp);
            reinterpret_cast<SettingsDialog*>(lp)->OnInit(hwnd);
            return TRUE;

        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDOK:
                    if (self) self->OnOk(hwnd);
                    EndDialog(hwnd, IDOK);
                    return TRUE;
                case IDCANCEL:
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
                case IDC_MANUAL_ONLY:
                    if (self) {
                        bool manual = IsDlgButtonChecked(hwnd, IDC_MANUAL_ONLY) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_REINDEX_INTERVAL), !manual);
                        EnableWindow(GetDlgItem(hwnd, IDC_REINDEX_UNIT),     !manual);
                    }
                    return TRUE;
                case IDC_EXCL_ADD:    if (self) self->OnAddExclusion(hwnd);    return TRUE;
                case IDC_EXCL_REMOVE: if (self) self->OnRemoveExclusion(hwnd); return TRUE;
            }
            break;
    }
    return FALSE;
}

void SettingsDialog::OnInit(HWND hwnd) {
    SetWindowTextW(hwnd, L"winindex — Settings");

    // Drive list
    HWND hDriveList = GetDlgItem(hwnd, IDC_DRIVE_LIST);
    auto selected = m_settings->GetSelectedDrives();
    for (const auto& drive : m_drives) {
        std::wstring label = drive.root;
        if (!drive.label.empty()) label += L" (" + drive.label + L")";
        label += (drive.filesystem == DriveFilesystem::NTFS) ? L" [NTFS]" : L" [FAT32]";

        int idx = static_cast<int>(
            SendMessageW(hDriveList, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(label.c_str())));

        bool isSel = std::find(selected.begin(), selected.end(), drive.root) != selected.end();
        SendMessageW(hDriveList, LB_SETSEL, isSel ? TRUE : FALSE, idx);
    }

    // Reindex interval
    uint64_t interval = m_settings->GetReindexIntervalHours();
    bool manualOnly = (interval == kReindexManualOnly);
    CheckDlgButton(hwnd, IDC_MANUAL_ONLY, manualOnly ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(hwnd, IDC_REINDEX_INTERVAL), !manualOnly);
    EnableWindow(GetDlgItem(hwnd, IDC_REINDEX_UNIT),     !manualOnly);

    HWND hUnit = GetDlgItem(hwnd, IDC_REINDEX_UNIT);
    SendMessageW(hUnit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Hours"));
    SendMessageW(hUnit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Days"));

    if (!manualOnly) {
        if (interval % 24 == 0 && interval >= 24) {
            SetDlgItemTextW(hwnd, IDC_REINDEX_INTERVAL,
                             std::to_wstring(interval / 24).c_str());
            SendMessageW(hUnit, CB_SETCURSEL, 1, 0); // Days
        } else {
            SetDlgItemTextW(hwnd, IDC_REINDEX_INTERVAL,
                             std::to_wstring(interval).c_str());
            SendMessageW(hUnit, CB_SETCURSEL, 0, 0); // Hours
        }
    } else {
        SetDlgItemTextW(hwnd, IDC_REINDEX_INTERVAL, L"48");
        SendMessageW(hUnit, CB_SETCURSEL, 0, 0);
    }

    // Exclusion list
    HWND hExclList = GetDlgItem(hwnd, IDC_EXCL_LIST);
    for (const auto& excl : m_settings->GetExcludedPaths()) {
        SendMessageW(hExclList, LB_ADDSTRING, 0,
                      reinterpret_cast<LPARAM>(excl.c_str()));
    }
}

void SettingsDialog::OnOk(HWND hwnd) {
    // Drives
    HWND hDriveList = GetDlgItem(hwnd, IDC_DRIVE_LIST);
    int count = static_cast<int>(SendMessageW(hDriveList, LB_GETCOUNT, 0, 0));
    std::vector<std::wstring> selected;
    for (int i = 0; i < count && i < static_cast<int>(m_drives.size()); ++i) {
        if (SendMessageW(hDriveList, LB_GETSEL, i, 0) > 0)
            selected.push_back(m_drives[i].root);
    }
    m_settings->SetSelectedDrives(selected);

    // Interval
    bool manualOnly = IsDlgButtonChecked(hwnd, IDC_MANUAL_ONLY) == BST_CHECKED;
    if (manualOnly) {
        m_settings->SetReindexIntervalHours(kReindexManualOnly);
    } else {
        wchar_t buf[32]{};
        GetDlgItemTextW(hwnd, IDC_REINDEX_INTERVAL, buf, 32);
        uint64_t val = _wtoi64(buf);
        int unitSel = static_cast<int>(
            SendMessageW(GetDlgItem(hwnd, IDC_REINDEX_UNIT), CB_GETCURSEL, 0, 0));
        if (unitSel == 1) val *= 24;
        m_settings->SetReindexIntervalHours(val > 0 ? val : kReindexDefaultHours);
    }

    // Exclusions
    HWND hExclList = GetDlgItem(hwnd, IDC_EXCL_LIST);
    int exclCount = static_cast<int>(SendMessageW(hExclList, LB_GETCOUNT, 0, 0));
    std::vector<std::wstring> excls;
    for (int i = 0; i < exclCount; ++i) {
        int len = static_cast<int>(SendMessageW(hExclList, LB_GETTEXTLEN, i, 0));
        std::wstring s(len, L'\0');
        SendMessageW(hExclList, LB_GETTEXT, i, reinterpret_cast<LPARAM>(s.data()));
        excls.push_back(std::move(s));
    }
    m_settings->SetExcludedPaths(excls);
}

void SettingsDialog::OnAddExclusion(HWND hwnd) {
    // Browse for folder
    wchar_t path[MAX_PATH]{};
    BROWSEINFOW bi{};
    bi.hwndOwner = hwnd;
    bi.pszDisplayName = path;
    bi.lpszTitle = L"Select folder to exclude:";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t fullPath[MAX_PATH]{};
        if (SHGetPathFromIDListW(pidl, fullPath)) {
            HWND hList = GetDlgItem(hwnd, IDC_EXCL_LIST);
            SendMessageW(hList, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(fullPath));
        }
        CoTaskMemFree(pidl);
    }
}

void SettingsDialog::OnRemoveExclusion(HWND hwnd) {
    HWND hList = GetDlgItem(hwnd, IDC_EXCL_LIST);
    int sel = static_cast<int>(SendMessageW(hList, LB_GETCURSEL, 0, 0));
    if (sel != LB_ERR) SendMessageW(hList, LB_DELETESTRING, sel, 0);
}

} // namespace winindex
