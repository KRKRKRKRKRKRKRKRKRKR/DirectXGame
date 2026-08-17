#pragma once
#include "../../IComponent.h"
#include "../../../../Math/Easing.h"

// パーティクルの「テンプレート」に付ける設定コンポーネント。ReflexEnemyComponent::isTemplateと
// 同じ考え方で、実際にシーン上に配置したこのGameObject自身は描画・破棄されるだけの
// 見本であり、PlayScene::SpawnParticleBurstAt等の複製処理がここに設定した値だけを読み取って
// ParticleComponent一式を新規生成する（このGameObject自身は複製されない）
class ParticleEmitterComponent : public IComponent {
public:
	int count = 12; // 1回の発生で飛び散らせる粒子の数

	// 初期サイズ・終了サイズはどちらも[min, max]からパーティクルごとに個別抽選する
	// （粒子ごとに大きさがばらつく）。終了サイズを0にすると縮んで消えるように見える
	float sizeStartMin = 0.2f;
	float sizeStartMax = 0.4f;
	float sizeEndMin = 0.0f;
	float sizeEndMax = 0.05f;

	// 飛び散る速度（1秒あたりの移動量）の範囲。方向は粒子ごとに球面上の一様ランダムを使う
	float speedMin = 3.0f;
	float speedMax = 8.0f;

	float lifeTime = 0.6f; // 全粒子共通の寿命（秒）
	Easing::Type sizeEasing = Easing::Type::kOutCubic; // サイズ変化のイージング種別

	// trueの場合、各粒子にRotatorComponentを付け、XYZ軸それぞれ独立にrotationSpeedMin〜Maxの
	// 範囲・符号（方向）もランダムで回転させ続ける（RotatorComponent::Randomizeと同じ抽選方式）。
	// 粒子ごとに個別抽選のため、同じ発生でも軸・速度・回転方向がばらける
	bool  enableRotation = false;
	float rotationSpeedMin = 90.0f;  // ランダム抽選する回転速度の下限（絶対値、度/秒）
	float rotationSpeedMax = 360.0f; // ランダム抽選する回転速度の上限（絶対値、度/秒）

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["count"] = count;
		out["sizeStartMin"] = sizeStartMin;
		out["sizeStartMax"] = sizeStartMax;
		out["sizeEndMin"] = sizeEndMin;
		out["sizeEndMax"] = sizeEndMax;
		out["speedMin"] = speedMin;
		out["speedMax"] = speedMax;
		out["lifeTime"] = lifeTime;
		out["sizeEasing"] = static_cast<int>(sizeEasing);
		out["enableRotation"] = enableRotation;
		out["rotationSpeedMin"] = rotationSpeedMin;
		out["rotationSpeedMax"] = rotationSpeedMax;
	}
	void FromJson(const nlohmann::json& in) override {
		count = in.value("count", count);
		sizeStartMin = in.value("sizeStartMin", sizeStartMin);
		sizeStartMax = in.value("sizeStartMax", sizeStartMax);
		sizeEndMin = in.value("sizeEndMin", sizeEndMin);
		sizeEndMax = in.value("sizeEndMax", sizeEndMax);
		speedMin = in.value("speedMin", speedMin);
		speedMax = in.value("speedMax", speedMax);
		lifeTime = in.value("lifeTime", lifeTime);
		sizeEasing = static_cast<Easing::Type>(in.value("sizeEasing", static_cast<int>(sizeEasing)));
		enableRotation = in.value("enableRotation", enableRotation);
		rotationSpeedMin = in.value("rotationSpeedMin", rotationSpeedMin);
		rotationSpeedMax = in.value("rotationSpeedMax", rotationSpeedMax);
	}
};
