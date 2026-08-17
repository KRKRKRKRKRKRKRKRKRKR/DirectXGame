#pragma once
#include "../../IComponent.h"

// 箱の中をピンポン玉のように跳ね回る敵の移動ロジック。壁のColliderは再利用しない
// （ColliderComponentBase::isTriggerは1つのColliderに1値のみで、「壁には物理的に跳ね返るが、
// プレイヤーにはすり抜けてトリガー判定させたい」という敵に必要な2種類の挙動を1つのColliderで
// 両立できないため）。代わりに座標の単純な範囲比較だけで箱の境界内に留める
// （Math/Collision.hの複雑な形状交差は使わず、学習コスト・デバッグのしやすさを優先）
class EnemyBounceComponent : public IComponent {
public:
	bool enabled = true;

	// 現在の移動速度（各軸独立、境界に当たった軸だけ符号反転する）
	Vector3 velocity = { 3.0f, 2.0f, 0.0f };

	// 跳ね回る範囲（ボックス）の中心と半径サイズ。ステージの壁の内側に収まるよう
	// エディタで箱のサイズに合わせて設定する
	Vector3 boundsCenter = { 0.0f, 0.0f, 0.0f };
	Vector3 boundsHalfExtent = { 4.0f, 4.0f, 0.5f };

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
