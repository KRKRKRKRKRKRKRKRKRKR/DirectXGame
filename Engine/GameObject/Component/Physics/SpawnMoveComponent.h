#pragma once
#include "../../IComponent.h"
#include "../../../../Math/Easing.h"

// スポーン演出用：GameObjectをstartPosからtargetPosへ、duration秒かけてイージングで移動させる。
// 到達したらfinishedをtrueにするだけで自分からは何もしない。
//
// 敵テンプレートに「+ コンポーネントを追加」から直接付けておくことで、PlayScene::SpawnEnemyAtが
// zOffset/duration/easingの設定値だけを複製し、startPos（=スポーン地点+Z方向オフセット）・
// targetPos（=本来のスポーン地点）は複製先ごとに動的に計算して上書きする（テンプレート自身の
// startPos/targetPosは実際には使われない、あくまで設定値の置き場所）
class SpawnMoveComponent : public IComponent {
public:
	// 開始位置を「本来のスポーン地点からZ方向にどれだけ離すか」で指定する（PlayScene側が
	// targetPos = 本来のスポーン地点、startPos = targetPos + {0,0,zOffset} を計算して使う）
	float zOffset = 10.0f;
	float duration = 0.5f;   // 移動にかける秒数
	Easing::Type easing = Easing::Type::kOutCubic;

	Vector3 startPos = { 0.0f, 0.0f, 0.0f };  // 実行時に複製先で上書きされる（テンプレート上は未使用）
	Vector3 targetPos = { 0.0f, 0.0f, 0.0f }; // 同上
	float elapsed = 0.0f;    // 経過時間（内部状態。保存しない＝復元時は必ず0から）

	// durationに達したらtrue。以降Updateは何もしない。
	// 既定値はtrue：PlayScene::SpawnEnemyAtが複製直後にfalseへ明示的に戻して演出を開始する。
	// シーン保存→ロードされたケース（startPos/targetPosは保存されずデフォルト{0,0,0}のまま
	// 復元される）でfalseのままだと、再生開始した瞬間Update()がLerp(startPos={0,0,0},
	// targetPos={0,0,0}, t)を計算して敵が原点へワープしてしまう。「保存されたTransformは
	// 既に演出完了後の最終位置」という前提のため、既定でtrueにして安全側に倒す
	bool finished = true;

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["zOffset"] = zOffset;
		out["duration"] = duration;
		out["easing"] = static_cast<int>(easing);
	}
	void FromJson(const nlohmann::json& in) override {
		zOffset = in.value("zOffset", zOffset);
		duration = in.value("duration", duration);
		easing = static_cast<Easing::Type>(in.value("easing", static_cast<int>(easing)));
		// 保存されたシーンをロードした場合は演出済み扱いにする（上のfinishedのコメント参照）
		finished = true;
	}
};
