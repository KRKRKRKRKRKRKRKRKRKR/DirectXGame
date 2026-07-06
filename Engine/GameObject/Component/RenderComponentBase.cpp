#include "RenderComponentBase.h"
#include "../../../Externals/imgui/imgui.h"
#include <string>

void RenderComponentBase::DrawImGui(const char* namePrefix) {
	std::string lightingLabel = std::string(namePrefix) + " Lighting";
	ImGui::Checkbox(lightingLabel.c_str(), &lighting);

	std::string colorLabel = std::string(namePrefix) + " Color";
	ImGui::ColorEdit4(colorLabel.c_str(), &color.x);

	// BlendMode選択コンボ。表示名の並びは BlendMode.h のenum定義順と対応させること
	static const char* kBlendModeNames[] = { "None", "Normal (Alpha)", "Add", "Subtract", "Multiply", "Screen" };
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
