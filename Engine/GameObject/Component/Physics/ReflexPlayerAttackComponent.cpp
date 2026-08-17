#include "ReflexPlayerAttackComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void ReflexPlayerAttackComponent::DrawImGui(const char* namePrefix) {
	std::string label = std::string(namePrefix) + "攻撃力";
	ImGui::DragFloat(label.c_str(), &attackPower, 0.5f, 0.0f, 100.0f);
}

void ReflexPlayerAttackComponent::ToJson(nlohmann::json& out) const {
	out["attackPower"] = attackPower;
}

void ReflexPlayerAttackComponent::FromJson(const nlohmann::json& in) {
	attackPower = in.value("attackPower", attackPower);
}

REGISTER_SIMPLE_COMPONENT(ReflexPlayerAttackComponent, "ReflexPlayerAttack", "REFLEXプレイヤー攻撃力", "物理");
