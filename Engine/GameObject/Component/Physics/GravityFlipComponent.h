#pragma once
#include "../../IComponent.h"
#include "GravityDirection.h"

// GraviTwistの核となる「4方向重力反転」コンポーネント。既存GravityComponent（Y軸専用、
// ColliderSystemの着地リセット処理と密結合）は無改造のまま残し、これは完全に別クラスとして
// 新設する。理由は2つ：(1) GravityComponentは他シーンで使われている可能性があり、
// Y軸専用という前提を変えると挙動が変わるリスクがある。(2) このコンポーネントは
// 「方向転換の残り回数」「着地イベント通知」等GraviTwist専有の状態を多く持つため、
// 無理に共通化するとGravityComponent側にも不要なフィールドが増えてしまう
class GravityFlipComponent : public IComponent {
public:
	bool  enabled = true;

	// 現在の重力方向＝現在の"床"側。JSON保存対象（設定値）
	GravityDirection direction = GravityDirection::kDown;
	float gravity = 20.0f;
	float maxFallSpeed = 30.0f;

	// 敵撃破時の回復（RegainDirectionChange）がクランプされる上限。JSON保存対象（設定値）
	int maxDirectionChange = 1;

	// 着地した瞬間に回復する方向転換回数。参考実装GraviTwistBeta（Player::OnGroundMove）では
	// maxDirectionChangeと食い違う値(2)がハードコードされており、着地時と敵撃破時で回復量が
	// 非対称になっていた。ユーザーの希望によりこの挙動をそのまま踏襲する。JSON保存対象（設定値）
	int landingDirectionChangeBonus = 2;

	// ここから下は実行中の一時状態（GravityComponent::velocityYと同じ扱いで保存しない。
	// ロード時は常に「静止・着地済み・残り回数満タン(landingDirectionChangeBonus分)」から再開する）
	float velocity = 0.0f;
	int   directionChangesRemaining = 2;
	bool  isGrounded = true;

	// 着地するたびに+1される「版数」。WaveManager（ウェーブ進行、フェーズ3で実装予定）が
	// 着地イベントの購読に使う。カメラ回転演出は廃止されたため現状の購読者はいないが、
	// 「1回のジャンプが終わった」という着地イベント自体はGraviTwistのコア仕様として残す
	int landingEventId = 0;

	void Update(float deltaTime, Transform& transform) override;

	// 敵に体当たりした際にEnemyComponentから呼ばれる。参考実装のRecoverCount()と同じく
	// 「1回分だけ回復し、maxDirectionChangeを超えないようクランプ」する（着地時の回復量
	// landingDirectionChangeBonusより低い上限になる非対称な仕様）
	void RegainDirectionChange() {
		directionChangesRemaining++;
		if (directionChangesRemaining > maxDirectionChange) directionChangesRemaining = maxDirectionChange;
	}

	// ColliderSystemが「落下方向と逆に押し戻された（＝着地した）」瞬間だけ呼ぶ。
	// inputLockRemainingをinputLockDurationへリセットする（＝ここから入力ロック開始）
	void NotifyLanded();

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
