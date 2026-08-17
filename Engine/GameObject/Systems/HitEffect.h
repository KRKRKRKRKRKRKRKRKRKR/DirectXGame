#pragma once
#include "../../../Math/MathTypes.h"

// 敵ヒット時のカメラシェイク・ヒットストップ要求を仲介するグローバル状態。
// ReflexEnemyComponent::OnTriggerEnter（当たり判定コールバック、UpdateContextを持たない）から
// SceneBase::Render（毎フレームカメラ/deltaTimeを確定する場所）へ「ヒットが起きた」ことを
// 伝える手段がないため、Easingと同じ「静的関数だけのnamespace」として仲介させる
namespace HitEffect {

	// 敵ヒット時にOnTriggerEnter側から呼ぶ。strengthはカメラを揺らす最大オフセット量、
	// durationは効果の継続時間（秒）。同じフレームに複数回呼ばれた場合は、より強い/長い方を採用する
	// （wait演出が重複しても短い方に上書きされて弱まらないようにするため）
	void RequestShake(float strength, float duration);
	void RequestHitStop(float duration);

	// SceneBase::Renderが毎フレーム呼ぶ。realDeltaTimeだけ両タイマーを消化し、
	// 「今フレームのシェイクオフセット（ランダムな向きに残り強さ分だけ揺らす）」と
	// 「今フレームがヒットストップ中かどうか」を返す
	Vector3 ConsumeShakeOffset(float realDeltaTime);
	bool IsHitStopActive(float realDeltaTime);

}
