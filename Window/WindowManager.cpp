#include "WindowManager.h"

WindowManager::~WindowManager() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClass(wc_.lpszClassName, wc_.hInstance);
}

void WindowManager::Create(const std::wstring& title,
    int32_t width, int32_t height) {
    clientWidth_ = width;
    clientHeight_ = height;

    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = L"DirectXGame";
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc_);

    RECT wrc = { 0, 0, clientWidth_, clientHeight_ };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    hwnd_ = CreateWindow(
        wc_.lpszClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr, nullptr,
        wc_.hInstance,
        nullptr
    );

    ShowWindow(hwnd_, SW_SHOW);
}

bool WindowManager::ProcessMessage() {
    MSG msg{};

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT) {
            return false;  // 終了
        }
    }
    return true;  // 継続
}

LRESULT CALLBACK WindowManager::WindowProc(
    HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}