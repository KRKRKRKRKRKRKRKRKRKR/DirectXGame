#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>


//ウィンドウプロシージャ
LRESULT CALLBACK windowPrec(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	//メッセージの処理
	switch (msg) {
	case WM_DESTROY:

		//ウィンドウが破棄されたときの処理
		PostQuitMessage(0);
		return 0;
	}

	//標準のメッセージ処理
	return DefWindowProc(hwnd, msg, wparam, lparam);
}


/// <summary>
/// ウィンドウの作成
/// </summary>
void CreateGameWindow() {

	//ウィンドウクラスの登録
	WNDCLASS wc{};

	//ウィンドウプロシージャー
	wc.lpfnWndProc = windowPrec;

	//クラス名
	wc.lpszClassName = L"DirectXGame";

	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);

	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスを登録する
	RegisterClass(&wc);

	//ウィンドウサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	//ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//クライアント領域を元に実際のサイズに変換する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの作成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,		//クラス名
		L"DirectXGame",			//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,	//ウィンドウスタイル
		CW_USEDEFAULT,			//表示X座標
		CW_USEDEFAULT,			//表示Y座標
		wrc.right - wrc.left,	//ウィンドウ幅
		wrc.bottom - wrc.top,	//ウィンドウ高
		nullptr,				//親ウィンドウハンドル
		nullptr,				//メニューハンドル
		wc.hInstance,			//インスタンスハンドル
		nullptr					//追加パラメーター
	);

	//ウィンドウの表示
	ShowWindow(hwnd, SW_SHOW);
}

void Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}

//文字列変換関数
std::wstring ConvertString(const std::string& str);
std::string ConvertString(const std::wstring& wstr);


int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	CreateGameWindow();

	int enemyHP = 100;
	Log(std::format("enemyHP{}\n", enemyHP));

//	Log(ConvertString(std::format(L"WSTRING{}\n",enemyHP)));

	MSG msg{};
	//メッセージループ
	while (msg.message != WM_QUIT) {
		//windowからのメッセージがあるかどうかをチェックする
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

			TranslateMessage(&msg);
			DispatchMessage(&msg);

		}
		else {
			//ゲームの処理

		}
	}

	return 0;

}



