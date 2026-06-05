#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <commctrl.h>

#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    // Enable Common Controls v6
    INITCOMMONCONTROLSEX icex{sizeof(icex), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icex);

    if (!winindex::MainWindow::Register(hInst))
        return 1;

    HWND hwnd = winindex::MainWindow::Create(hInst);
    if (!hwnd)
        return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
