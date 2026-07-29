#include "Game.h"
#include "../Externals/imgui/imgui.h"
#include "../Engine/GameObject/ComponentRegistration.h"
#include "../Engine/Utils/EditorState.h"

void Game::Initialize(Renderer* renderer, Camera* camera, Window* window) {
	renderer_ = renderer;
	camera_ = camera;
	window_ = window;
	RegisterEngineComponents(); // JSON保存/復元のためのコンポーネント型登録（シーン初期化前に一度だけ）
	sceneManager_.Initialize(renderer, camera, SceneType::kTitle);
}

void Game::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	// ImGuiパネル上でのマウス操作（クリック・ドラッグ・ホイールスクロール等）中はカメラの
	// Orbit/Look/Pan/Dollyとして拾わないようにする（Hierarchyのスクロールでカメラが前後に
	// 動いてしまう、パネルのドラッグでカメラが回転してしまう等を防ぐ）
	if (EditorState::GetInstance().IsUiVisible() && !ImGui::GetIO().WantCaptureMouse) {
		camera_->HandleInput(deltaTime);
	}

	// 0.5秒ごとに直近フレームの平均FPS/フレーム時間を計算し直す（毎フレーム表示は変動が激しく読みづらいため）
	fpsSampleTimer_ += deltaTime;
	fpsSampleFrames_++;
	constexpr float kFpsSampleInterval = 0.5f;
	if (fpsSampleTimer_ >= kFpsSampleInterval) {
		fpsDisplayValue_    = static_cast<float>(fpsSampleFrames_) / fpsSampleTimer_;
		frameTimeDisplayMs_ = (fpsSampleTimer_ / static_cast<float>(fpsSampleFrames_)) * 1000.0f;
		fpsSampleTimer_  = 0.0f;
		fpsSampleFrames_ = 0;
	}
}

void Game::Render() {
	sceneManager_.Render(deltaTime_);
	if (EditorState::GetInstance().IsUiVisible()) {
		DrawImGui();

		// アプリを閉じようとした（×ボタン/Alt+F4でWindow::closeRequested_が立った）場合、
		// エディタUI表示中だけ保存確認モーダルを挟む
		if (window_ && window_->IsCloseRequested() && !showQuitSavePrompt_) {
			showQuitSavePrompt_ = true;
			ImGui::OpenPopup("アプリを閉じる確認##QuitSavePrompt");
		}
		if (showQuitSavePrompt_) {
			DrawQuitSavePrompt();
		}
	} else if (window_ && window_->IsCloseRequested()) {
		// UI非表示（Releaseビルド既定・F11で隠した実プレイ中）はモーダルを表示する手段が無いため、
		// 保存確認自体はエディタ専用機能と割り切り、これまで通り即座に閉じる
		window_->ConfirmClose();
	}
}

void Game::DrawQuitSavePrompt() {
	if (ImGui::BeginPopupModal("アプリを閉じる確認##QuitSavePrompt", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("現在のシーンを保存してから閉じますか？");
		ImGui::Separator();
		if (ImGui::Button("保存して閉じる", ImVec2(140, 0))) {
			if (IScene* scene = sceneManager_.GetCurrentScene()) scene->RequestSave();
			window_->ConfirmClose();
			showQuitSavePrompt_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("保存せず閉じる", ImVec2(140, 0))) {
			window_->ConfirmClose();
			showQuitSavePrompt_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(100, 0))) {
			window_->CancelCloseRequest();
			showQuitSavePrompt_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void Game::DrawImGui() {
	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.1f（0.5秒平均）", fpsDisplayValue_);
	ImGui::Text("フレーム時間: %.3f ms（0.5秒平均）", frameTimeDisplayMs_);
	ImGui::Text("瞬間FPS: %.1f", 1.0f / deltaTime_);
	ImGui::End();

	ImGui::Begin("カメラ##Camera");
	ImGui::Text("視点回転: 右ボタン + マウス移動");
	ImGui::Text("周回: Alt + 左ボタン + マウス移動");
	ImGui::Text("平行移動: 中ボタン + マウス移動");
	ImGui::Text("前後移動: マウスホイール");
	ImGui::Text("pos.x = %.1f", camera_->GetCameraData().position.x);
	ImGui::Text("pos.y = %.1f", camera_->GetCameraData().position.y);
	ImGui::Text("pos.z= %.1f", camera_->GetCameraData().position.z);
	ImGui::End();
}
