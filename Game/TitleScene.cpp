#include "TitleScene.h"
#include "../Externals/imgui/imgui.h"
#include "../Engine/InputDevice/InputDevice.h"

void TitleScene::Initialize(Renderer* renderer, Camera* camera) {
	renderer_ = renderer;
	camera_ = camera;
	nextScene_ = SceneType::kNone;
}

void TitleScene::Render(float deltaTime) {
	(void)deltaTime;

	ImGui::Begin("Title");
	ImGui::Text("ClearRoot Engine");
	ImGui::Text("Press ENTER to continue");
	ImGui::End();

	if (Input::IsTriggered(DIK_RETURN)) {
		nextScene_ = SceneType::kSelect;
	}
}
