#include "ReflexEnemyHealthBarComponent.h"
#include "../../ComponentRegistry.h"
#include "ReflexEnemyComponent.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>

void ReflexEnemyHealthBarComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)deltaTime;
	if (!target_ || !ctx.renderer) return;

	// 敵が消える直前（pendingDestroy）はもう表示する意味がないので描かない
	if (target_->pendingDestroy) return;

	float ratio = (target_->maxHp > 0.0f) ? (target_->hp / target_->maxHp) : 0.0f;
	ratio = (std::max)(0.0f, (std::min)(1.0f, ratio));

	Vector3 barCenter = transform.translation;
	barCenter.y += heightOffset;

	// 背景（灰色、常にwidthいっぱい）
	Transform backgroundTransform;
	backgroundTransform.translation = barCenter;
	backgroundTransform.scale = { width, height, 0.05f };
	ctx.renderer->DrawCube(backgroundTransform, backgroundColor, kTextureNone, false);

	if (ratio <= 0.0f) return;

	// 前景（HP割合分の幅。中心を左端に揃えるため、幅の縮み分だけXをマイナス方向へずらす）
	float fillWidth = width * ratio;
	Transform fillTransform;
	fillTransform.translation = { barCenter.x - (width - fillWidth) * 0.5f, barCenter.y, barCenter.z };
	fillTransform.scale = { fillWidth, height, 0.06f }; // 背景よりわずかに手前（Z-fighting防止）
	ctx.renderer->DrawCube(fillTransform, fillColor, kTextureNone, false);
}

void ReflexEnemyHealthBarComponent::DrawImGui(const char* namePrefix) {
	std::string widthLabel = std::string(namePrefix) + "バーの幅";
	ImGui::DragFloat(widthLabel.c_str(), &width, 0.05f, 0.1f, 5.0f);

	std::string heightLabel = std::string(namePrefix) + "バーの高さ";
	ImGui::DragFloat(heightLabel.c_str(), &height, 0.01f, 0.02f, 1.0f);

	std::string offsetLabel = std::string(namePrefix) + "浮かせる高さ";
	ImGui::DragFloat(offsetLabel.c_str(), &heightOffset, 0.05f, 0.0f, 5.0f);

	std::string bgLabel = std::string(namePrefix) + "背景色";
	ImGui::ColorEdit4(bgLabel.c_str(), &backgroundColor.x);

	std::string fillLabel = std::string(namePrefix) + "HP色";
	ImGui::ColorEdit4(fillLabel.c_str(), &fillColor.x);
}

void ReflexEnemyHealthBarComponent::ToJson(nlohmann::json& out) const {
	out["width"] = width;
	out["height"] = height;
	out["heightOffset"] = heightOffset;
	out["backgroundColor"] = Vector4ToJson(backgroundColor);
	out["fillColor"] = Vector4ToJson(fillColor);
}

void ReflexEnemyHealthBarComponent::FromJson(const nlohmann::json& in) {
	width = in.value("width", width);
	height = in.value("height", height);
	heightOffset = in.value("heightOffset", heightOffset);
	if (in.contains("backgroundColor")) backgroundColor = Vector4FromJson(in["backgroundColor"]);
	if (in.contains("fillColor")) fillColor = Vector4FromJson(in["fillColor"]);
}
