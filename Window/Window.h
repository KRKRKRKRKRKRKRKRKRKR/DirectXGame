#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
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

	// ゲッター
	HWND    GetHWND()        const { return hwnd_; }
	int32_t GetClientWidth() const { return clientWidth_; }
	int32_t GetClientHeight()const { return clientHeight_; }

private:
	// ウィンドウプロシージャはstaticである必要がある
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
		WPARAM wparam, LPARAM lparam);

	HWND     hwnd_ = nullptr;
	WNDCLASS wc_ = {};
	int32_t  clientWidth_;
	int32_t  clientHeight_ ;
};