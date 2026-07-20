#include "TESTComponent.h"
#include "../../Engine/GameObject/ComponentRegistry.h"
#include "../../Externals/imgui/imgui.h"

void TESTComponent::Update(float deltaTime, Transform& transform) {
	(void)deltaTime; (void)transform;
	// TODO: 毎フレームの処理
}

void TESTComponent::DrawImGui(const char* namePrefix) {
	(void)namePrefix;
	// TODO: Inspectorに表示する項目
}

void TESTComponent::ToJson(nlohmann::json& out) const {
	(void)out;
	// TODO: 保存するフィールド
}

void TESTComponent::FromJson(const nlohmann::json& in) {
	(void)in;
	// TODO: 読み込むフィールド
}

REGISTER_SIMPLE_COMPONENT(TESTComponent, "TEST", "TEST", "その他");
