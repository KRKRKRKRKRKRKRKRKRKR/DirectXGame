#pragma once
#include "../../Engine/GameObject/IComponent.h"

// TODO: このコンポーネントの説明を書く
class TESTComponent : public IComponent {
public:
	void Update(float deltaTime, Transform& transform) override;
	void DrawImGui(const char* namePrefix) override;
	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
