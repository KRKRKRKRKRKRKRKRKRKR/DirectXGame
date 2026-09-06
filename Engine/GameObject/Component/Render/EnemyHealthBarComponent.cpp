// 10DaysJam
#include "EnemyHealthBarComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <string>

namespace {
	// GridItemSpawnComponent.cppのkTypeNamesと同じ表記（Inspector上で同じ名前に揃える）
	constexpr const char* kItemTypeNames[] = { "赤", "緑", "青" };
	constexpr int kItemTypeCount = static_cast<int>(sizeof(kItemTypeNames) / sizeof(kItemTypeNames[0]));
}

void EnemyHealthBarComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	if (!ctx.renderer) return;

	float targetRatio = (maxCollectCount > 0)
		? 1.0f - static_cast<float>(currentCollectCount_) / static_cast<float>(maxCollectCount)
		: 0.0f;
	targetRatio = (std::max)(0.0f, (std::min)(1.0f, targetRatio));

	// displayRatio_をtargetRatioへなめらかに近づける（ReflexPlayerComponent等と同じ
	// 「差分をdeltaTime*speedぶんだけ縮める」補間。取得数が減ることは無いため常に減少方向のみ）
	float diff = targetRatio - displayRatio_;
	float step = shrinkSpeed * deltaTime;
	if (std::abs(diff) <= step) {
		displayRatio_ = targetRatio;
	} else {
		displayRatio_ += (diff > 0.0f ? step : -step);
	}

	Vector3 barCenter = transform.translation;

	// 背景（灰色、常にwidthいっぱい。ReflexEnemyHealthBarComponentと同じDrawCube2枚方式）
	Transform backgroundTransform;
	backgroundTransform.translation = barCenter;
	backgroundTransform.scale = { width, height, height };
	ctx.renderer->DrawCube(backgroundTransform, backgroundColor, kTextureNone, false);

	if (displayRatio_ <= 0.0f) return;

	// 前景（残量割合分の幅。中心を左端に揃えるため、幅の縮み分だけXをマイナス方向へずらす。
	// 背景よりわずかに手前(+Z)に出してZ-fighting防止）
	float fillWidth = width * displayRatio_;
	Transform fillTransform;
	fillTransform.translation = { barCenter.x - (width - fillWidth) * 0.5f, barCenter.y, barCenter.z + height * 0.1f };
	fillTransform.scale = { fillWidth, height, height };
	ctx.renderer->DrawCube(fillTransform, fillColor, kTextureNone, false);
}

void EnemyHealthBarComponent::DrawImGui(const char* namePrefix) {
	std::string maxLabel = std::string(namePrefix) + "満タンにする取得数";
	ImGui::DragInt(maxLabel.c_str(), &maxCollectCount, 1, 1, 999);

	std::string widthLabel = std::string(namePrefix) + "バーの幅";
	ImGui::DragFloat(widthLabel.c_str(), &width, 0.05f, 0.1f, 20.0f);

	std::string heightLabel = std::string(namePrefix) + "バーの高さ";
	ImGui::DragFloat(heightLabel.c_str(), &height, 0.01f, 0.02f, 5.0f);

	std::string speedLabel = std::string(namePrefix) + "減少アニメーション速度";
	ImGui::DragFloat(speedLabel.c_str(), &shrinkSpeed, 0.05f, 0.1f, 20.0f);

	std::string bgLabel = std::string(namePrefix) + "背景色";
	ImGui::ColorEdit4(bgLabel.c_str(), &backgroundColor.x);

	std::string fillLabel = std::string(namePrefix) + "残量色";
	ImGui::ColorEdit4(fillLabel.c_str(), &fillColor.x);

	ImGui::Separator();
	std::string watchLabel = std::string(namePrefix) + "監視するアイテム種別(取得すると減る)";
	ImGui::Text("%s", watchLabel.c_str());
	for (int t = 0; t < kItemTypeCount; ++t) {
		std::string checkboxLabel = std::string(namePrefix) + kItemTypeNames[t];
		bool watched = watchedTypes[static_cast<size_t>(t)];
		if (ImGui::Checkbox(checkboxLabel.c_str(), &watched)) {
			watchedTypes[static_cast<size_t>(t)] = watched;
		}
	}

	std::string countInfo = std::string(namePrefix) + "取得数: "
		+ std::to_string(currentCollectCount_) + " / " + std::to_string(maxCollectCount);
	ImGui::Text("%s", countInfo.c_str());
}

void EnemyHealthBarComponent::ToJson(nlohmann::json& out) const {
	out["maxCollectCount"] = maxCollectCount;
	out["width"] = width;
	out["height"] = height;
	out["shrinkSpeed"] = shrinkSpeed;
	out["backgroundColor"] = Vector4ToJson(backgroundColor);
	out["fillColor"] = Vector4ToJson(fillColor);
	nlohmann::json watched = nlohmann::json::array();
	for (bool w : watchedTypes) watched.push_back(w);
	out["watchedTypes"] = watched;
}

void EnemyHealthBarComponent::FromJson(const nlohmann::json& in) {
	maxCollectCount = in.value("maxCollectCount", maxCollectCount);
	width = in.value("width", width);
	height = in.value("height", height);
	shrinkSpeed = in.value("shrinkSpeed", shrinkSpeed);
	if (in.contains("backgroundColor")) backgroundColor = Vector4FromJson(in["backgroundColor"]);
	if (in.contains("fillColor")) fillColor = Vector4FromJson(in["fillColor"]);
	if (in.contains("watchedTypes")) {
		const auto& watched = in["watchedTypes"];
		for (size_t i = 0; i < watchedTypes.size() && i < watched.size(); ++i) {
			watchedTypes[i] = watched[i].get<bool>();
		}
	}
}

REGISTER_SIMPLE_COMPONENT(EnemyHealthBarComponent, "EnemyHealthBar", "敵HPバー", "描画");
