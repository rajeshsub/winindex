#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>
#include "../core/settings/Settings.h"
#include "../core/indexer/DriveEnumerator.h"

namespace winindex {

class FirstRunDialog {
public:
    FirstRunDialog(HWND hParent, std::shared_ptr<Settings> settings);
    bool Show(); // Returns true if user confirmed

private:
    HWND                       m_hParent;
    std::shared_ptr<Settings>  m_settings;
    std::vector<DriveInfo>     m_drives;

    static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void OnInit(HWND hwnd);
    void OnOk(HWND hwnd);
};

} // namespace winindex
