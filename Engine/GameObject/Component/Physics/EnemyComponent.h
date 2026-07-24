#pragma once
#include "../../IComponent.h"

// 敵1体ぶんの状態。フェーズ2時点ではプレイヤー接触への反応のみ実装し、カウントダウン/赤化/
// スコア加算はフェーズ3で追加する（HealthComponentと同じく「相手のレイヤーを見て反応する」
// OnTriggerEnterパターンを踏襲）
class EnemyComponent : public IComponent {
public:
	bool enabled = true;

	// ColliderSystem::ResolveAndDrawのループ中からOnTriggerEnterが呼ばれるため、その場で
	// GameObjectをunique_ptrごとeraseするとダングリングになる。フラグだけ立てておき、
	// ResolveAndDraw完了後（PlayScene::HandleSceneTransitionInput冒頭）にまとめて
	// DeleteObjectsで回収する「遅延破棄」パターンにする
	bool pendingDestroy = false;

	void OnTriggerEnter(GameObject& other) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
