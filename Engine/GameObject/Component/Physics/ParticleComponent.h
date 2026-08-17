#pragma once
#include "../../IComponent.h"
#include "../../../../Math/Easing.h"

// 1粒のパーティクル本体。等速直進（重力なし）で飛び続け、lifeTimeが経過したら
// pendingDestroyを立てて自壊する（ReflexEnemyComponentのpendingDestroyパターンを踏襲。
// ColliderSystem等ループ中からその場でGameObjectをeraseできないため、フラグだけ立てて
// 実際の破棄はシーン側が別タイミングでまとめて行う）。
// 見た目（CubeRenderComponent等）・当たり判定は持たない。サイズ変化はTransformComponentの
// scaleへ直接書き込むため、このGameObjectのTransform.scaleは他の用途に使わないこと
class ParticleComponent : public IComponent {
public:
	bool enabled = true;

	// 飛ぶ方向（正規化済み）と速度（1秒あたりの移動量）。ParticleEmitterComponent::Emitが
	// 全方位ランダムな方向・ランダムな速度で初期化してから使う
	Vector3 direction = { 0.0f, 1.0f, 0.0f };
	float   speed = 1.0f;

	// 開始/終了サイズ（一辺の長さ相当。CubeRenderComponentのscaleにそのまま使う想定）と、
	// その間を結ぶイージング種別。経過時間の比率tにEasing::Applyをかけた値でLerpする
	float        sizeStart = 0.3f;
	float        sizeEnd = 0.0f;
	Easing::Type sizeEasing = Easing::Type::kOutCubic;

	float lifeTime = 0.6f;    // 消えるまでの秒数
	float elapsed = 0.0f;     // 経過時間（内部状態。保存しない＝復元時は必ず0から）
	bool  pendingDestroy = false;

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;
};
