#pragma once
#include "../../IComponent.h"
#include "CollisionLayer.h"

// HP（体力）を持ち、指定レイヤーの相手とTrigger越しに重なった瞬間にダメージを受ける
// コンポーネント。Unityで言う「OnTriggerEnterの中でCompareTagして減算する」スクリプトに相当する。
// PlayerにAddComponentし、damageFromLayerをkObstacleのままにしておけば、
// 「障害物のTriggerに触れたらHPが0になる」がそのまま実現できる
class HealthComponent : public IComponent {
public:
	bool  enabled = true;
	float maxHp = 100.0f;
	float hp = 100.0f;

	// 触れたらダメージを与える相手の所属レイヤー（Unityの other.CompareTag("Obstacle") に相当）。
	// このレイヤーと一致しない相手（Item等）に触れても何も起きない
	CollisionLayer damageFromLayer = CollisionLayer::kObstacle;

	// true: damageを無視して即座にhpを0にする（Race the Sunの「障害物に触れたら即アウト」向け）。
	// false: damage分だけ減算する（弾を何発か受けたら死ぬ、のような段階的なHP制にしたい場合用）
	bool fatalDamage = true;
	float damage = 100.0f;

	void OnTriggerEnter(GameObject& other) override;

	bool IsDead() const { return hp <= 0.0f; }

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
