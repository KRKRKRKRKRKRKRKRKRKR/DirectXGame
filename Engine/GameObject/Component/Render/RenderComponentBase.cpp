#include "RenderComponentBase.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include <string>

// BlendMode選択コンボ。表示名の並びは BlendMode.h のenum定義順と対応させること。
// DrawImGuiとToJson/FromJsonの両方から使うためファイルスコープに置く
static const char* kBlendModeNames[] = { "None", "Normal (Alpha)", "Add", "Subtract", "Multiply", "Screen" };

void RenderComponentBase::DrawImGui(const char* namePrefix) {
	std::string lightingLabel = std::string(namePrefix) + " Lighting";
	ImGui::Checkbox(lightingLabel.c_str(), &lighting);

	std::string colorLabel = std::string(namePrefix) + " Color";
	ImGui::ColorEdit4(colorLabel.c_str(), &color.x);

	std::string blendModeLabel = std::string(namePrefix) + " BlendMode";
	int current = static_cast<int>(blendMode);
	if (ImGui::BeginCombo(blendModeLabel.c_str(), kBlendModeNames[current])) {
		for (int i = 0; i < static_cast<int>(BlendMode::kCount); i++) {
			bool selected = (i == current);
			if (ImGui::Selectable(kBlendModeNames[i], selected))
				blendMode = static_cast<BlendMode>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// ブレンドの強さ（commandList->OMSetBlendFactor()に渡す0〜1の定数）。
	// None/Multiply/ScreenはDestBlendがSrcColor依存かブレンド自体が無効なため、
	// この定数では強さを補間できない（GPU固定機能ブレンダーの制約）のでスライダーを無効化する
	std::string blendStrengthLabel = std::string(namePrefix) + " Blend Strength";
	bool effective = (blendMode == BlendMode::kNormal || blendMode == BlendMode::kAdd || blendMode == BlendMode::kSubtract);
	if (!effective) ImGui::BeginDisabled();
	ImGui::SliderFloat(blendStrengthLabel.c_str(), &blendStrength, 0.0f, 1.0f);
	if (!effective) ImGui::EndDisabled();

	// 2値抜き(Binary Alpha/αTest): αがしきい値未満のピクセルを描画しない
	std::string alphaTestLabel      = std::string(namePrefix) + " Alpha Test";
	std::string alphaThresholdLabel = std::string(namePrefix) + " Alpha Threshold";
	ImGui::Checkbox(alphaTestLabel.c_str(), &alphaTest);
	if (!alphaTest) ImGui::BeginDisabled();
	ImGui::SliderFloat(alphaThresholdLabel.c_str(), &alphaThreshold, 0.0f, 1.0f);
	if (!alphaTest) ImGui::EndDisabled();
}

void RenderComponentBase::ToJson(nlohmann::json& out) const {
	out["color"] = Vector4ToJson(color);
	out["lighting"] = lighting;
	out["blendMode"] = kBlendModeNames[static_cast<int>(blendMode)];
	out["blendStrength"] = blendStrength;
	out["alphaTest"] = alphaTest;
	out["alphaThreshold"] = alphaThreshold;
}

void RenderComponentBase::FromJson(const nlohmann::json& in) {
	if (in.contains("color")) color = Vector4FromJson(in["color"]);
	lighting = in.value("lighting", lighting);
	std::string blendModeName = in.value("blendMode", std::string(kBlendModeNames[0]));
	for (int i = 0; i < static_cast<int>(BlendMode::kCount); i++) {
		if (blendModeName == kBlendModeNames[i]) { blendMode = static_cast<BlendMode>(i); break; }
	}
	blendStrength = in.value("blendStrength", blendStrength);
	alphaTest = in.value("alphaTest", alphaTest);
	alphaThreshold = in.value("alphaThreshold", alphaThreshold);
}
