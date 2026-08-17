#pragma once
#include "../../IComponent.h"

// GameObjectを毎フレーム自動で回転させ続けるコンポーネント。敵オブジェクトの
// 待機モーションのような「常に回り続ける」演出向け。X/Y/Z軸それぞれ独立に
// 速度（度/秒）を持ち、符号で回転方向（正=+回転、負=-回転、0=その軸は回転しない）を選べる
class RotatorComponent : public IComponent {
public:
	bool  enabled = true;

	// 各軸の回転速度（度/秒）。符号が回転方向を表す（+で正回転、-で逆回転、0で停止）
	float speedX = 0.0f;
	float speedY = 90.0f;
	float speedZ = 0.0f;

	// trueの場合、Randomize()がrandomSpeedMin〜randomSpeedMaxの範囲でspeedX/Y/Zを
	// 引き直す（3軸それぞれ独立に抽選。符号込みなので方向もランダムになる）。
	// テンプレート側の設定として保存し、実際の抽選はスポーン処理側が明示的にRandomize()を
	// 呼んだタイミングで行う（AddComponent直後に自動で引いてしまうと、複製元テンプレートの
	// 値をそのまま複製したいケースと衝突するため）
	bool  randomizeOnSpawn = false;
	float randomSpeedMin = 30.0f;  // ランダム抽選する速度の下限（絶対値、度/秒）
	float randomSpeedMax = 180.0f; // ランダム抽選する速度の上限（絶対値、度/秒）

	// randomSpeedMin〜randomSpeedMaxの範囲でspeedX/Y/Zを軸ごとに独立に引き直す。
	// 各軸、符号もランダム（50%の確率で反転）にすることで回転方向もばらつかせる
	void Randomize();

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["enabled"] = enabled;
		out["speedX"] = speedX;
		out["speedY"] = speedY;
		out["speedZ"] = speedZ;
		out["randomizeOnSpawn"] = randomizeOnSpawn;
		out["randomSpeedMin"] = randomSpeedMin;
		out["randomSpeedMax"] = randomSpeedMax;
	}
	void FromJson(const nlohmann::json& in) override {
		enabled = in.value("enabled", enabled);
		speedX = in.value("speedX", speedX);
		speedY = in.value("speedY", speedY);
		speedZ = in.value("speedZ", speedZ);
		randomizeOnSpawn = in.value("randomizeOnSpawn", randomizeOnSpawn);
		randomSpeedMin = in.value("randomSpeedMin", randomSpeedMin);
		randomSpeedMax = in.value("randomSpeedMax", randomSpeedMax);
	}
};
