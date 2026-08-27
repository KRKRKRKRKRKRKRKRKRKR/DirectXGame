#include "DashedLineComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void DashedLineComponent::DrawImGui(const char* namePrefix) {
	std::string countLabel = std::string(namePrefix) + "本数";
	std::string widthLabel = std::string(namePrefix) + "1本の長さ";
	std::string thicknessLabel = std::string(namePrefix) + "太さ";
	std::string spacingLabel = std::string(namePrefix) + "間隔（中心間の距離）";
	std::string colorLabel = std::string(namePrefix) + "色";

	ImGui::DragInt(countLabel.c_str(), &dashCount, 0.1f, 1, 50);
	ImGui::DragFloat(widthLabel.c_str(), &dashWidth, 0.01f, 0.01f, 10.0f);
	ImGui::DragFloat(thicknessLabel.c_str(), &dashThickness, 0.005f, 0.01f, 5.0f);
	ImGui::DragFloat(spacingLabel.c_str(), &dashSpacing, 0.01f, 0.01f, 10.0f);
	ImGui::ColorEdit4(colorLabel.c_str(), &color.x);
}

void DashedLineComponent::ToJson(nlohmann::json& out) const {
	out["dashCount"] = dashCount;
	out["dashWidth"] = dashWidth;
	out["dashThickness"] = dashThickness;
	out["dashSpacing"] = dashSpacing;
	out["color"] = Vector4ToJson(color);
}

void DashedLineComponent::FromJson(const nlohmann::json& in) {
	dashCount = in.value("dashCount", dashCount);
	dashWidth = in.value("dashWidth", dashWidth);
	dashThickness = in.value("dashThickness", dashThickness);
	dashSpacing = in.value("dashSpacing", dashSpacing);
	if (in.contains("color")) color = Vector4FromJson(in["color"]);
	// lastBuiltDashCount等はあえて復元しない（AlphabetTextComponent::lastBuiltTextと同じ理由。
	// 空/不一致値のままにしておくことで、Load直後の最初のUpdateDashedLineComponentsで
	// 必ず子GameObjectが作り直される）
}

REGISTER_SIMPLE_COMPONENT(DashedLineComponent, "DashedLine", "破線", "描画");
