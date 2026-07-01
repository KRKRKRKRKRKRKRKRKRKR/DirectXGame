#include "Window.h"
#include "../../Externals/imgui/imgui.h"
#include "../../Externals/imgui/imgui_impl_dx12.h"
#include "../../Externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window* Window::instance_ = nullptr;

Window::~Window() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClass(wc_.lpszClassName, wc_.hInstance);
}

void Window::Create(const std::wstring& title,int32_t width, int32_t height) {
    instance_ = this;
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

bool Window::ProcessMessage() {
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

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

     if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
	 }

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        // 枠ドラッグ中は同じサイズでWM_SIZEが連続するため、最大化/リストアなど
        // ドラッグを伴わない変化のみここで即時反映する（ドラッグ中の分は下のWM_EXITSIZEMOVEで処理）
        if (instance_ && wparam != SIZE_MINIMIZED) {
            int32_t width = LOWORD(lparam);
            int32_t height = HIWORD(lparam);
            if (width > 0 && height > 0 && (width != instance_->clientWidth_ || height != instance_->clientHeight_)) {
                if (wparam == SIZE_MAXIMIZED || wparam == SIZE_RESTORED) {
                    instance_->clientWidth_ = width;
                    instance_->clientHeight_ = height;
                    if (instance_->onResize_) {
                        instance_->onResize_(width, height);
                    }
                }
            }
        }
        return 0;
    case WM_EXITSIZEMOVE:
        // 枠ドラッグでのリサイズが終わったタイミングでスワップチェーン等を更新する
        if (instance_) {
            RECT rect{};
            GetClientRect(hwnd, &rect);
            int32_t width = rect.right - rect.left;
            int32_t height = rect.bottom - rect.top;
            if (width > 0 && height > 0 && (width != instance_->clientWidth_ || height != instance_->clientHeight_)) {
                instance_->clientWidth_ = width;
                instance_->clientHeight_ = height;
                if (instance_->onResize_) {
                    instance_->onResize_(width, height);
                }
            }
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

