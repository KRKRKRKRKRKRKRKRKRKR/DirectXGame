#include "Engine.h"
#include "../Debug/Debug.h"
#include "Utils/Logger.h"
#include "Utils/EditorState.h"
#include "../Externals/imgui/imgui.h"

namespace {
	// ゲーム画面のアスペクト比をウィンドウ形状によらず16:9に固定する。エディタUI表示中は
	// ドック中央ノードの矩形、非表示中（実際のプレイ画面そのもの）はウィンドウ全体を「外枠」として、
	// その中に収まる最大の中央寄せ矩形を計算する（レターボックス/ピラーボックス）
	constexpr float kFixedAspectRatio = 16.0f / 9.0f;

	void FitAspectRect(float outerX, float outerY, float outerWidth, float outerHeight, float targetAspect,
		float& x, float& y, float& width, float& height) {
		float outerAspect = outerWidth / outerHeight;
		if (outerAspect > targetAspect) {
			// 外枠の方が横長 → 高さいっぱいに合わせ、左右に黒帯（ピラーボックス）
			height = outerHeight;
			width = height * targetAspect;
		} else {
			// 外枠の方が縦長 → 幅いっぱいに合わせ、上下に黒帯（レターボックス）
			width = outerWidth;
			height = width / targetAspect;
		}
		x = outerX + (outerWidth - width) * 0.5f;
		y = outerY + (outerHeight - height) * 0.5f;
	}
}

void Engine::Initialize(const std::wstring& windowTitle, int width, int height) {
	Debug::RegisterCrashHandler();
	Logger::Initialize();
	window_.Create(windowTitle, width, height);
	Debug::EnableDebugLayer();
	directX_.Initialize(window_.GetHWND(), window_.GetClientWidth(), window_.GetClientHeight());
	renderer_.Initialize(directX_.GetDevice(), directX_.GetCommandList(), directX_.GetDescriptorHeaps(), window_.GetClientWidth(), window_.GetClientHeight());
	directX_.WaitForGPUCompletion();
	renderer_.SetCommandList(directX_.GetCommandList());
	InputDevice::GetInstance().Initialize(window_.GetInstance(), window_.GetHWND());
	Debug::SetupInfoQueue(directX_.GetDevice());
	camera_.Initialize({0.0f, 0.5f, -5.0f});
	imgui_.Initialize(window_.GetHWND(), &directX_);
	AudioManager::GetInstance().Initialize();
	deltaTime_.Start();

	window_.SetResizeCallback([this](int32_t width, int32_t height) {
		directX_.Resize(width, height);
		renderer_.Resize(width, height);
	});
}

bool Engine::Update() {
	if (!window_.ProcessMessage()) return false;
	InputDevice::GetInstance().Update();
	AudioManager::GetInstance().Update(); // OneShotVoice（HitSoundComponent等）の再生完了ボイス破棄
	deltaTime_.Update();

	// F11でエディタUIの表示/非表示を切り替える（Debugビルドのみ）。何かにテキスト入力中の
	// 誤操作は無視する。Releaseビルドではこの分岐自体をコンパイルしないことで、
	// 起動時に隠したエディタUIをプレイヤーがF11で復元できないようにする
#ifndef NDEBUG
	if (!ImGui::GetIO().WantCaptureKeyboard && Input::IsTriggered(DIK_F11)) {
		EditorState::GetInstance().ToggleUiVisible();
	}
#endif

	// 前フレームのEndFrame()（WaitForFence込み）が完了した直後のこのタイミングでは、GPUは
	// 前フレームまでの全コピー操作を完了済みのため、TextureManager::UpdatePixelsが退避していた
	// 古いintermediateResourceを安全に解放できる（詳細はReleasePendingTextureUpdatesのコメント参照）
	renderer_.ReleasePendingTextureUpdates();

	directX_.BeginFrame();
	renderer_.ResetFrameIndex();
	renderer_.SetCommandList(directX_.GetCommandList());
	renderer_.SetBackBufferTarget(directX_.GetRTVHandle(), directX_.GetDSVHandle());

	bool uiVisible = EditorState::GetInstance().IsUiVisible();
	imgui_.BeginFrame(uiVisible);

	// ドック中央ノード（Sceneビュー、UI非表示時はウィンドウ全体）を「外枠」として、その中に
	// 16:9で収まる中央寄せ矩形を3D描画のビューポート/シザーへ反映する。directX_.BeginFrame()が
	// 既にフルウィンドウのビューポートを設定済みだが、これより後に呼ぶことでコマンドリスト上で
	// 上書きされる。UI非表示時（F11トグル/Releaseビルド既定）は実際のプレイ画面そのものなので、
	// ここでもアスペクト比を固定しないと本来の目的（ゲーム画面の見た目を保つ）を果たせない
	float outerX, outerY, outerWidth, outerHeight;
	if (uiVisible) {
		imgui_.GetSceneViewportRect(outerX, outerY, outerWidth, outerHeight);
	} else {
		outerX = 0.0f;
		outerY = 0.0f;
		outerWidth  = static_cast<float>(window_.GetClientWidth());
		outerHeight = static_cast<float>(window_.GetClientHeight());
	}
	if (outerWidth > 0.0f && outerHeight > 0.0f) {
		float x, y, width, height;
		FitAspectRect(outerX, outerY, outerWidth, outerHeight, kFixedAspectRatio, x, y, width, height);
		renderer_.SetSceneViewportRect(x, y, width, height);
	}

	return true;
}

void Engine::Flush() {
	renderer_.FlushAll();
	imgui_.EndFrame(&directX_);
	directX_.EndFrame();
}

void Engine::Finalize() {
	// GPUが直前フレームのコマンド（Present等）を実行完了する前に、
	// ImGuiやRendererがGPUリソース（フォントテクスチャ等）を解放してしまわないよう、
	// 他の終了処理より先にGPU完了を待つ（STATE_CREATION WARNING対策）
	directX_.WaitForGPUCompletion();

	AudioManager::GetInstance().Finalize();
	imgui_.Finalize();
	renderer_.Finalize();
	InputDevice::GetInstance().Finalize();
	directX_.Finalize();
}