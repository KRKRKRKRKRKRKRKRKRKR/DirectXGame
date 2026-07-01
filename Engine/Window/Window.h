#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <functional>
class Window {
public:
	Window() = default;
	~Window();

	// ウィンドウの作成
	void Create(const std::wstring& title = L"DirectXGame",
		int32_t width = 1280,
		int32_t height = 720);

	// メッセージ処理（trueの間ループ継続）
	bool ProcessMessage();

	// リサイズ完了時に呼ばれるコールバック（新しいクライアント幅・高さを渡す）
	void SetResizeCallback(std::function<void(int32_t, int32_t)> callback) { onResize_ = std::move(callback); }

	// ゲッター
	HWND    GetHWND()        const { return hwnd_; }
	int32_t GetClientWidth() const { return clientWidth_; }
	int32_t GetClientHeight()const { return clientHeight_; }
	HINSTANCE GetInstance()     const { return wc_.hInstance; }
private:
	// ウィンドウプロシージャはstaticである必要がある
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
		WPARAM wparam, LPARAM lparam);

	HWND     hwnd_ = nullptr;
	WNDCLASS wc_ = {};
	int32_t  clientWidth_;
	int32_t  clientHeight_ ;
	std::function<void(int32_t, int32_t)> onResize_;

	static Window* instance_; // WindowProc（static）からインスタンスへアクセスするため
};