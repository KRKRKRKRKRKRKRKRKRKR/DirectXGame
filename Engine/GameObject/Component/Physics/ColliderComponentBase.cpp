#include "ColliderComponentBase.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <string>

// 表示名の並びはCollisionLayer.hのenum定義順と対応させること。DrawImGuiとToJson/FromJsonの
// 両方から使うためファイルスコープに置く。
// kCollisionLayerNamesはJSON保存・FromJsonでの文字列照合キーのため英語のまま維持し、
// UI表示だけkCollisionLayerDisplayNames（日本語、同じ並び順）を使う
static const char* kCollisionLayerNames[] = { "Default", "Player", "Obstacle", "Item", "Environment", "Enemy" };
static const char* kCollisionLayerDisplayNames[] = { "デフォルト", "プレイヤー", "障害物", "アイテム", "環境", "敵" };
// 要素数だけはコンパイル時に検証できる（順序の対応まではチェックできないため、追加時は
// 配列とenumの両方を必ず同じ位置に増やすこと）
static_assert(sizeof(kCollisionLayerNames) / sizeof(kCollisionLayerNames[0]) == static_cast<size_t>(CollisionLayer::kCount),
	"kCollisionLayerNames must have exactly CollisionLayer::kCount entries, in the same order as the enum");
static_assert(sizeof(kCollisionLayerDisplayNames) / sizeof(kCollisionLayerDisplayNames[0]) == static_cast<size_t>(CollisionLayer::kCount),
	"kCollisionLayerDisplayNames must have exactly CollisionLayer::kCount entries, in the same order as the enum");

void ColliderComponentBase::DrawImGui(const char* namePrefix) {
	std::string layerLabel = std::string(namePrefix) + "コライダーレイヤー";
	int current = static_cast<int>(layer);
	if (ImGui::BeginCombo(layerLabel.c_str(), kCollisionLayerDisplayNames[current])) {
		for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
			bool selected = (i == current);
			if (ImGui::Selectable(kCollisionLayerDisplayNames[i], selected))
				layer = static_cast<CollisionLayer>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	std::string triggerLabel = std::string(namePrefix) + "トリガーにする";
	ImGui::Checkbox(triggerLabel.c_str(), &isTrigger);

	std::string staticLabel = std::string(namePrefix) + "静的（動かさない）";
	ImGui::Checkbox(staticLabel.c_str(), &isStatic);

	// 衝突対象レイヤーのチェックボックス群。「所属レイヤー」とは別概念で、
	// 「このコライダーがどのレイヤーと衝突判定するか」を個別に選べるようにする。
	// ShouldLayersCollideは両者が互いに相手を選んでいる場合のみtrueを返す
	std::string treeLabel = std::string(namePrefix) + "衝突対象レイヤー";
	if (ImGui::TreeNode(treeLabel.c_str())) {
		for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
			ImGui::Checkbox(kCollisionLayerDisplayNames[i], &collidesWith[i]);
		}
		ImGui::TreePop();
	}
}

void ColliderComponentBase::ToJson(nlohmann::json& out) const {
	out["layer"] = kCollisionLayerNames[static_cast<int>(layer)];
	out["isTrigger"] = isTrigger;
	out["isStatic"] = isStatic;
	for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
		out["collidesWith"].push_back(collidesWith[i]);
	}
}

void ColliderComponentBase::FromJson(const nlohmann::json& in) {
	std::string layerName = in.value("layer", std::string(kCollisionLayerNames[0]));
	for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
		if (layerName == kCollisionLayerNames[i]) { layer = static_cast<CollisionLayer>(i); break; }
	}
	isTrigger = in.value("isTrigger", isTrigger);
	isStatic  = in.value("isStatic", isStatic);
	if (in.contains("collidesWith")) {
		int i = 0;
		for (const auto& v : in["collidesWith"]) {
			if (i >= static_cast<int>(CollisionLayer::kCount)) break;
			collidesWith[i++] = v.get<bool>();
		}
	}
}
