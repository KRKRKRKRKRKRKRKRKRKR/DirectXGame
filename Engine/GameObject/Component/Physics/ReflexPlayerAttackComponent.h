#pragma once
#include "../../IComponent.h"

// プレイヤーの攻撃力だけを持つコンポーネント。ReflexPlayerComponent（移動・イージング設定）
// とは責務を分け、攻撃関連の設定が増えてきてもReflexPlayerComponent本体を肥大化させない。
// ReflexEnemyComponent::OnTriggerEnterが、接触した相手（プレイヤー）からこの値を読み取り、
// 自分のHPから減算する
class ReflexPlayerAttackComponent : public IComponent {
public:
	float attackPower = 10.0f;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
