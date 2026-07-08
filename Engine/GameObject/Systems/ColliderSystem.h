#pragma once
#include "../../../Math/MathTypes.h"
#include <vector>

class GameObject;
class Renderer;

// 複数のGameObjectを横断する「当たり判定の総当り解決システム」。特定のGameSceneの内容
// （Cube/Floor/Sphere等が何であるか）には一切依存しない汎用ロジックのため、1オブジェクトに
// 付与するIComponent派生（Engine/GameObject/Component/）とは別に、Engine/GameObject/Systems/に置く。
// フレームをまたぐ状態は持たない（毎フレーム引数を渡して呼ぶだけの軽量クラス）
class ColliderSystem {
public:
	// targets内でCollider（Sphere/OBB）を持つ全オブジェクトについて、ColliderComponentBase::
	// ShouldLayersCollide（お互いが相手の所属レイヤーを自分のcollidesWithに含めている場合のみ
	// true）を通過したペアの重なりを判定する。両方Solid（isTrigger=false）で重なっている場合は
	// Collision::*Penetrationで押し戻し量を求め、Transform.translationを半分ずつ動かして分離する
	// （isTriggerがどちらかtrueなら押し戻さず検知のみ。isPlaying=falseの間は検知・色分けのみ行い
	// 押し戻しはしない）。判定・押し戻し後の位置でワイヤーフレームを緑（重なりなし）/黄（Trigger
	// 込みの重なり）/赤（Solid同士の重なり）に色分けして描画する
	void ResolveAndDraw(const std::vector<GameObject*>& targets, bool isPlaying,
		Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj);
};
