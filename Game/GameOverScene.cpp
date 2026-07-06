#include "GameOverScene.h"
#include "../Externals/imgui/imgui.h"
#include "../Engine/InputDevice/InputDevice.h"

void GameOverScene::Initialize(Renderer* renderer, Camera* camera) {
	renderer_ = renderer;
	camera_ = camera;
	nextScene_ = SceneType::kNone;
}

void GameOverScene::Render(float deltaTime) {
	(void)deltaTime;

	ImGui::Begin("Game Over");
	ImGui::Text("GAME OVER");
	ImGui::Text("Press ENTER to return to Title");
	ImGui::End();

	if (Input::IsTriggered(DIK_RETURN)) {
		nextScene_ = SceneType::kTitle;
	}
}
