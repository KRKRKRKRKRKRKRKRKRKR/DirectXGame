#include "SelectScene.h"
#include "../Externals/imgui/imgui.h"
#include "../Engine/InputDevice/InputDevice.h"

void SelectScene::Initialize(Renderer* renderer, Camera* camera) {
	renderer_ = renderer;
	camera_ = camera;
	nextScene_ = SceneType::kNone;
}

void SelectScene::Render(float deltaTime) {
	(void)deltaTime;

	ImGui::Begin("Select");
	ImGui::Text("Select Stage");
	ImGui::Text("Press ENTER to play, ESC to go back");
	ImGui::End();

	if (Input::IsTriggered(DIK_RETURN)) {
		nextScene_ = SceneType::kPlay;
	} else if (Input::IsTriggered(DIK_ESCAPE)) {
		nextScene_ = SceneType::kTitle;
	}
}
