#pragma once
#include "../IComponent.h"

// GameObjectに重力落下を付与するコンポーネント。速度を積算してTransformのYを
// 動かすだけの単純な実装で、地面判定自体は行わない。実際に地面で止まるには、
// このGameObjectが (1) Collider（Sphere/OBB）を持ち、(2) Solid（isTrigger=false）で、
// (3) 地面側のレイヤーとcollidesWithが噛み合っていて、(4) gizmoTargets_に登録されている、
// という既存の当たり判定の前提を満たす必要がある（PlayScene::ResolveAndDrawColliderGizmosの
// 押し戻し処理が地面との重なりを検出して押し戻すことで着地する）
class GravityComponent : public IComponent {
public:
	bool  enabled = true;
	float gravity = 9.8f;      // 重力加速度
	float velocityY = 0.0f;    // 現在の落下速度（内部状態。着地時の押し戻しでリセットされる）
	float maxFallSpeed = 20.0f; // 終端速度（下向きの落下速度の上限）。無制限に加速すると
	                             // 1フレームの移動量が薄い床の厚みを超えてすり抜ける原因になるため上限を設ける

	void Update(float deltaTime, Transform& transform) override;
	void DrawImGui(const char* namePrefix) override;
};
