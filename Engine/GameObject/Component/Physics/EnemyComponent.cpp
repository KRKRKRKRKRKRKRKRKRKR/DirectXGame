#include "EnemyComponent.h"
#include "ColliderComponentBase.h"
#include "GravityFlipComponent.h"
#include "../../GameObject.h"
#include "../../ComponentRegistry.h"

void EnemyComponent::OnTriggerEnter(GameObject& other) {
	if (!enabled || pendingDestroy) return;

	// 相手のColliderComponentBase::layerを見て、プレイヤーとの接触かどうかを判定する
	// （HealthComponent::OnTriggerEnterと同じ「相手のレイヤーを見て自分が反応する」パターン）
	auto* otherCollider = other.GetComponent<ColliderComponentBase>();
	if (!otherCollider) return;
	if (otherCollider->layer != CollisionLayer::kPlayer) return;

	pendingDestroy = true;

	// 体当たりで倒すと、プレイヤーの空中での方向転換回数が1回分回復する
	if (auto* flip = other.GetComponent<GravityFlipComponent>()) {
		flip->RegainDirectionChange();
	}
}

void EnemyComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
}

void EnemyComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	pendingDestroy = false;
}

REGISTER_SIMPLE_COMPONENT(EnemyComponent, "Enemy", "敵", "物理");
