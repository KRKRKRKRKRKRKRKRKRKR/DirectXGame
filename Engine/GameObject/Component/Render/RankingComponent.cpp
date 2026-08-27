#include "RankingComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void RankingComponent::DrawImGui(const char* namePrefix) {
	std::string startYLabel = std::string(namePrefix) + "1行目のYオフセット";
	std::string spacingLabel = std::string(namePrefix) + "行の間隔（RankingCameraScrollerと同じ値にする）";
	std::string zLabel = std::string(namePrefix) + "Zオフセット";
	std::string charScaleLabel = std::string(namePrefix) + "文字サイズ";
	std::string charSpacingLabel = std::string(namePrefix) + "文字間隔";
	std::string rankColumnLabel = std::string(namePrefix) + "順位列のXオフセット";
	std::string nameColumnLabel = std::string(namePrefix) + "名前列のXオフセット";
	std::string scoreColumnLabel = std::string(namePrefix) + "スコア列のXオフセット";

	ImGui::DragFloat(startYLabel.c_str(), &rowStartY, 0.05f, -50.0f, 50.0f);
	ImGui::DragFloat(spacingLabel.c_str(), &rowSpacing, 0.01f, 0.05f, 10.0f);
	ImGui::DragFloat(zLabel.c_str(), &rowZ, 0.05f, -50.0f, 50.0f);
	ImGui::DragFloat(charScaleLabel.c_str(), &charScale, 0.01f, 0.05f, 10.0f);
	ImGui::DragFloat(charSpacingLabel.c_str(), &charSpacing, 0.01f, 0.05f, 10.0f);
	ImGui::DragFloat(rankColumnLabel.c_str(), &rankColumnX, 0.05f, -50.0f, 50.0f);
	ImGui::DragFloat(nameColumnLabel.c_str(), &nameColumnX, 0.05f, -50.0f, 50.0f);
	ImGui::DragFloat(scoreColumnLabel.c_str(), &scoreColumnX, 0.05f, -50.0f, 50.0f);

	// 確認なし・即時リセット（ユーザー指定）。押した瞬間にresetRequested_を立てるだけで、
	// 実際のRankingManager::Reset()呼び出しはRankingScene側が次フレームで行う
	std::string resetLabel = std::string(namePrefix) + "ランキングをリセット";
	if (ImGui::Button(resetLabel.c_str())) {
		resetRequested_ = true;
	}
}

void RankingComponent::ToJson(nlohmann::json& out) const {
	out["rowStartY"] = rowStartY;
	out["rowSpacing"] = rowSpacing;
	out["rowZ"] = rowZ;
	out["charScale"] = charScale;
	out["charSpacing"] = charSpacing;
	out["rankColumnX"] = rankColumnX;
	out["nameColumnX"] = nameColumnX;
	out["scoreColumnX"] = scoreColumnX;
}

void RankingComponent::FromJson(const nlohmann::json& in) {
	rowStartY = in.value("rowStartY", rowStartY);
	rowSpacing = in.value("rowSpacing", rowSpacing);
	rowZ = in.value("rowZ", rowZ);
	charScale = in.value("charScale", charScale);
	charSpacing = in.value("charSpacing", charSpacing);
	rankColumnX = in.value("rankColumnX", rankColumnX);
	nameColumnX = in.value("nameColumnX", nameColumnX);
	scoreColumnX = in.value("scoreColumnX", scoreColumnX);
	// lastBuilt*はあえて復元しない（AlphabetTextComponent::lastBuiltTextと同じ理由。
	// ロード直後は必ず一度は再構築されるようにするため）
}

REGISTER_SIMPLE_COMPONENT(RankingComponent, "Ranking", "ランキング表示", "描画");
