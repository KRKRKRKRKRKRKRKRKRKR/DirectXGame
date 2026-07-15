#include "RenderComponentBase.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include <string>

// BlendMode選択コンボ。表示名の並びは BlendMode.h のenum定義順と対応させること。
// DrawImGuiとToJson/FromJsonの両方から使うためファイルスコープに置く。
// kBlendModeNamesはJSON保存・FromJsonでの文字列照合キーのため英語のまま維持し、
// UI表示だけkBlendModeDisplayNames（日本語、同じ並び順）を使う
static const char* kBlendModeNames[] = { "None", "Normal (Alpha)", "Add", "Subtract", "Multiply", "Screen" };
static const char* kBlendModeDisplayNames[] = { "なし", "通常（アルファ）", "加算", "減算", "乗算", "スクリーン" };
// 要素数だけはコンパイル時に検証できる（順序の対応まではチェックできないため、追加時は
// 配列とenumの両方を必ず同じ位置に増やすこと）
static_assert(sizeof(kBlendModeNames) / sizeof(kBlendModeNames[0]) == static_cast<size_t>(BlendMode::kCount),
	"kBlendModeNames must have exactly BlendMode::kCount entries, in the same order as the enum");
static_assert(sizeof(kBlendModeDisplayNames) / sizeof(kBlendModeDisplayNames[0]) == static_cast<size_t>(BlendMode::kCount),
	"kBlendModeDisplayNames must have exactly BlendMode::kCount entries, in the same order as the enum");

void RenderComponentBase::DrawImGui(const char* namePrefix) {
	std::string lightingLabel = std::string(namePrefix) + "ライティング";
	ImGui::Checkbox(lightingLabel.c_str(), &lighting);

	std::string colorLabel = std::string(namePrefix) + "色";
	ImGui::ColorEdit4(colorLabel.c_str(), &color.x);

	std::string blendModeLabel = std::string(namePrefix) + "ブレンドモード";
	int current = static_cast<int>(blendMode);
	if (ImGui::BeginCombo(blendModeLabel.c_str(), kBlendModeDisplayNames[current])) {
		for (int i = 0; i < static_cast<int>(BlendMode::kCount); i++) {
			bool selected = (i == current);
			if (ImGui::Selectable(kBlendModeDisplayNames[i], selected))
				blendMode = static_cast<BlendMode>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	// ブレンドの強さ（commandList->OMSetBlendFactor()に渡す0〜1の定数）。
	// None/Multiply/ScreenはDestBlendがSrcColor依存かブレンド自体が無効なため、
	// この定数では強さを補間できない（GPU固定機能ブレンダーの制約）のでスライダーを無効化する
	std::string blendStrengthLabel = std::string(namePrefix) + "ブレンドの強さ";
	bool effective = (blendMode == BlendMode::kNormal || blendMode == BlendMode::kAdd || blendMode == BlendMode::kSubtract);
	if (!effective) ImGui::BeginDisabled();
	ImGui::SliderFloat(blendStrengthLabel.c_str(), &blendStrength, 0.0f, 1.0f);
	if (!effective) ImGui::EndDisabled();

	// 2値抜き(Binary Alpha/αTest): αがしきい値未満のピクセルを描画しない
	std::string alphaTestLabel      = std::string(namePrefix) + "アルファテスト";
	std::string alphaThresholdLabel = std::string(namePrefix) + "アルファしきい値";
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
