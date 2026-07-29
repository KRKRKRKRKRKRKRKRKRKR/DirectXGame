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

	// ウィンドウを閉じようとしたか（×ボタン/Alt+F4等でWM_CLOSEを受け取ったか）。
	// WM_CLOSEを受け取ってもここでは即座にDestroyWindowしない（保存確認モーダルを挟むため）。
	// 呼び出し側（Game）が毎フレームこれを見て、確認が済んだらConfirmClose/CancelCloseRequestを呼ぶ
	bool IsCloseRequested() const { return closeRequested_; }

	// ユーザーが「保存して閉じる/保存せず閉じる」を選んだ後に呼ぶ。ここでは即座にDestroyWindow
	// しない。呼び出し側（Game::Render、ImGuiモーダルのボタンハンドラ内）はDirectXの
	// コマンドリスト記録中・ImGuiフレームの途中であり、その最中にウィンドウ（＝スワップチェーンの
	// 出力先）を破棄するとPresent/フレーム終端処理が不正な状態になりかねないため、実際の
	// DestroyWindowはProcessMessage()の先頭（フレームが何も進行していない安全なタイミング）まで
	// 遅延させる
	void ConfirmClose() { pendingDestroy_ = true; }

	// ユーザーが「キャンセル」を選んだ後に呼ぶ。closeRequested_をfalseへ戻し、通常運転に戻す
	void CancelCloseRequest() { closeRequested_ = false; }

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
	bool     closeRequested_ = false;
	bool     pendingDestroy_ = false; // ConfirmClose()で立ち、ProcessMessage()の先頭で実際に破棄する

	static Window* instance_; // WindowProc（static）からインスタンスへアクセスするため
};