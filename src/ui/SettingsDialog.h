#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../core/indexer/DriveEnumerator.h"
#include "../core/settings/Settings.h"
#include <memory>

namespace winindex {

class SettingsDialog {
public:
    SettingsDialog(HWND hParent, std::shared_ptr<Settings> settings);
    bool Show();

private:
    HWND m_hParent;
    std::shared_ptr<Settings> m_settings;

    static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void OnInit(HWND hwnd);
    void OnOk(HWND hwnd);
    static void OnAddLocation(HWND hwnd);
    static void OnRemovePath(HWND hwnd);
    static void OnAddExclusion(HWND hwnd);
    static void OnRemoveExclusion(HWND hwnd);
};

}  // namespace winindex
