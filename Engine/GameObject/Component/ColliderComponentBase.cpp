#include "ColliderComponentBase.h"
#include "../../../Externals/imgui/imgui.h"
#include <string>

void ColliderComponentBase::DrawImGui(const char* namePrefix) {
	// 表示名の並びはCollisionLayer.hのenum定義順と対応させること
	static const char* kCollisionLayerNames[] = { "Default", "Player", "Obstacle", "Item", "Environment" };

	std::string layerLabel = std::string(namePrefix) + " Collider Layer";
	int current = static_cast<int>(layer);
	if (ImGui::BeginCombo(layerLabel.c_str(), kCollisionLayerNames[current])) {
		for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
			bool selected = (i == current);
			if (ImGui::Selectable(kCollisionLayerNames[i], selected))
				layer = static_cast<CollisionLayer>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	std::string triggerLabel = std::string(namePrefix) + " Collider Is Trigger";
	ImGui::Checkbox(triggerLabel.c_str(), &isTrigger);

	std::string staticLabel = std::string(namePrefix) + " Collider Is Static";
	ImGui::Checkbox(staticLabel.c_str(), &isStatic);

	// 衝突対象レイヤーのチェックボックス群。「所属レイヤー」とは別概念で、
	// 「このコライダーがどのレイヤーと衝突判定するか」を個別に選べるようにする。
	// ShouldLayersCollideは両者が互いに相手を選んでいる場合のみtrueを返す
	std::string treeLabel = std::string(namePrefix) + " Collides With";
	if (ImGui::TreeNode(treeLabel.c_str())) {
		for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
			ImGui::Checkbox(kCollisionLayerNames[i], &collidesWith[i]);
		}
		ImGui::TreePop();
	}
}
