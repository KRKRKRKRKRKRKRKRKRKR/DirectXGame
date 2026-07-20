#include "HealthComponent.h"
#include "ColliderComponentBase.h"
#include "../../GameObject.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <string>

// レイヤー名の表示はColliderComponentBase.cppと同じ並び（CollisionLayer.hのenum順）。
// 共有テーブルはまだ無いため、このコンボ用にここだけ同じ内容を持つ
// （kCollisionLayerNames自体はJSON保存キーのため英語のまま、表示だけ日本語）
static const char* kHealthLayerNames[] = { "Default", "Player", "Obstacle", "Item", "Environment" };
static const char* kHealthLayerDisplayNames[] = { "デフォルト", "プレイヤー", "障害物", "アイテム", "環境" };
static_assert(sizeof(kHealthLayerNames) / sizeof(kHealthLayerNames[0]) == static_cast<size_t>(CollisionLayer::kCount),
	"kHealthLayerNames must have exactly CollisionLayer::kCount entries, in the same order as the enum");

void HealthComponent::OnTriggerEnter(GameObject& other) {
	if (!enabled || IsDead()) return;

	// 相手のColliderComponentBase::layerを見て、ダメージ対象かどうかを判定する
	// （Unityの if (other.CompareTag("Obstacle")) と同じ役割）
	auto* otherCollider = other.GetComponent<ColliderComponentBase>();
	if (!otherCollider) return;
	if (otherCollider->layer != damageFromLayer) return;

	hp = fatalDamage ? 0.0f : (std::max)(0.0f, hp - damage);
}

void HealthComponent::DrawImGui(const char* namePrefix) {
	std::string enabledLabel = std::string(namePrefix) + "有効化";
	std::string hpLabel      = std::string(namePrefix) + "HP";
	std::string maxHpLabel   = std::string(namePrefix) + "最大HP";
	std::string layerLabel   = std::string(namePrefix) + "ダメージ元レイヤー";
	std::string fatalLabel   = std::string(namePrefix) + "即死ダメージにする";
	std::string damageLabel  = std::string(namePrefix) + "ダメージ量";

	ImGui::Checkbox(enabledLabel.c_str(), &enabled);

	// 現在HPをひと目で見えるようにバー表示する（Unity Inspectorのカスタムバー相当）
	ImGui::ProgressBar(maxHp > 0.0f ? hp / maxHp : 0.0f, ImVec2(-1.0f, 0.0f));
	ImGui::DragFloat(hpLabel.c_str(), &hp, 1.0f, 0.0f, maxHp);
	ImGui::DragFloat(maxHpLabel.c_str(), &maxHp, 1.0f, 1.0f, 10000.0f);

	int current = static_cast<int>(damageFromLayer);
	if (ImGui::BeginCombo(layerLabel.c_str(), kHealthLayerDisplayNames[current])) {
		for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
			bool selected = (i == current);
			if (ImGui::Selectable(kHealthLayerDisplayNames[i], selected))
				damageFromLayer = static_cast<CollisionLayer>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox(fatalLabel.c_str(), &fatalDamage);
	if (!fatalDamage) {
		ImGui::DragFloat(damageLabel.c_str(), &damage, 1.0f, 0.0f, 10000.0f);
	}
}

void HealthComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
	out["maxHp"] = maxHp;
	out["hp"] = hp;
	out["damageFromLayer"] = kHealthLayerNames[static_cast<int>(damageFromLayer)];
	out["fatalDamage"] = fatalDamage;
	out["damage"] = damage;
}

void HealthComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	maxHp = in.value("maxHp", maxHp);
	hp = in.value("hp", hp);
	std::string layerName = in.value("damageFromLayer", std::string(kHealthLayerNames[static_cast<int>(damageFromLayer)]));
	for (int i = 0; i < static_cast<int>(CollisionLayer::kCount); i++) {
		if (layerName == kHealthLayerNames[i]) { damageFromLayer = static_cast<CollisionLayer>(i); break; }
	}
	fatalDamage = in.value("fatalDamage", fatalDamage);
	damage = in.value("damage", damage);
}

REGISTER_SIMPLE_COMPONENT(HealthComponent, "Health", "体力", "物理");
