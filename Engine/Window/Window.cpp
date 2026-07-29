#include "Window.h"
#include "../../Externals/imgui/imgui.h"
#include "../../Externals/imgui/imgui_impl_dx12.h"
#include "../../Externals/imgui/imgui_impl_win32.h"
#include "../../Resources/Icon/resource.h"

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
    wc_.lpszClassName = L"ClearRootEngine";
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc_.hIcon = LoadIcon(wc_.hInstance, MAKEINTRESOURCE(IDI_ICON1));
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
    // フレームが何も進行していないこの時点（BeginFrame/コマンドリスト記録の前）でだけ
    // 実際にDestroyWindowする。ConfirmClose()自体はImGuiモーダルのボタンハンドラ内
    // （DirectXのコマンドリスト記録中）から呼ばれるため、そこで即座に破棄すると
    // Present等のフレーム終端処理がまだ済んでいないスワップチェーンを壊すおそれがある
    if (pendingDestroy_) {
        pendingDestroy_ = false;
        if (hwnd_) DestroyWindow(hwnd_);
    }

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
    case WM_CLOSE:
        // 既定のDestroyWindowを呼ばず、いったん「閉じようとした」ことだけ記録する。
        // 実際に閉じるかどうか（保存確認モーダルの結果）はGame側が毎フレーム
        // IsCloseRequested()を見て判断し、ConfirmClose()/CancelCloseRequest()を呼び返す
        if (instance_) instance_->closeRequested_ = true;
        return 0;
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

