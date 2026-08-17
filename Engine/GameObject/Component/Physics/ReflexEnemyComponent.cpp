#include "ReflexEnemyComponent.h"
#include "ColliderComponentBase.h"
#include "ReflexPlayerAttackComponent.h"
#include "../../GameObject.h"
#include "../../ComponentRegistry.h"
#include "../../Systems/HitEffect.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>
#include <algorithm>

void ReflexEnemyComponent::OnTriggerEnter(GameObject& other) {
	if (!enabled || isTemplate || pendingDestroy) return;

	// 相手のColliderComponentBase::layerを見て、プレイヤーとの接触かどうかを判定する
	// （EnemyComponent::OnTriggerEnterと同じ「相手のレイヤーを見て自分が反応する」パターン）
	auto* otherCollider = other.GetComponent<ColliderComponentBase>();
	if (!otherCollider) return;
	if (otherCollider->layer != CollisionLayer::kPlayer) return;

	// ReflexPlayerAttackComponentを持たないプレイヤーに触れた場合は、後方互換として
	// 従来通り即死させる（攻撃力コンポーネントの導入前に組んだシーンでも動くようにするため）
	float attackPower = hp;
	if (auto* attack = other.GetComponent<ReflexPlayerAttackComponent>()) {
		attackPower = attack->attackPower;
	}
	hp = (std::max)(0.0f, hp - attackPower);
	pendingHitParticle = true; // 生死問わず、ダメージが入った瞬間は毎回被弾パーティクルを出す
	pendingHitSound = true;    // 同上、ヒットSEも生死問わず毎回鳴らす

	HitEffect::RequestShake(hitShakeStrength, hitShakeDuration);
	HitEffect::RequestHitStop(hitStopDuration);

	if (hp <= 0.0f) {
		pendingDestroy = true;
	}
}

void ReflexEnemyComponent::DrawImGui(const char* namePrefix) {
	std::string enabledLabel = std::string(namePrefix) + "有効";
	ImGui::Checkbox(enabledLabel.c_str(), &enabled);

	std::string templateLabel = std::string(namePrefix) + "敵テンプレートにする";
	ImGui::Checkbox(templateLabel.c_str(), &isTemplate);
	if (isTemplate) {
		ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s",
			(std::string(namePrefix) + "テンプレート（Play開始時に自動で隠されます）").c_str());
		ImGui::TextWrapped("%s", (std::string(namePrefix)
			+ "このオブジェクトのタグ名をReflexEnemySpawnerComponentの「テンプレートのタグ」に指定すると、"
			"見た目・当たり判定サイズ・ヒット演出パラメータが複製されます。").c_str());
	}

	std::string maxHpLabel = std::string(namePrefix) + "最大HP";
	if (ImGui::DragFloat(maxHpLabel.c_str(), &maxHp, 1.0f, 1.0f, 200.0f)) {
		hp = (std::min)(hp, maxHp);
	}
	std::string hpLabel = std::string(namePrefix) + "現在HP";
	ImGui::DragFloat(hpLabel.c_str(), &hp, 1.0f, 0.0f, maxHp);

	std::string shakeStrengthLabel = std::string(namePrefix) + "ヒットシェイク強さ";
	ImGui::DragFloat(shakeStrengthLabel.c_str(), &hitShakeStrength, 0.01f, 0.0f, 2.0f);

	std::string shakeDurationLabel = std::string(namePrefix) + "ヒットシェイク時間";
	ImGui::DragFloat(shakeDurationLabel.c_str(), &hitShakeDuration, 0.01f, 0.0f, 1.0f);

	std::string hitStopLabel = std::string(namePrefix) + "ヒットストップ時間";
	ImGui::DragFloat(hitStopLabel.c_str(), &hitStopDuration, 0.005f, 0.0f, 0.5f);
}

void ReflexEnemyComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
	out["isTemplate"] = isTemplate;
	out["maxHp"] = maxHp;
	out["hp"] = hp;
	out["hitShakeStrength"] = hitShakeStrength;
	out["hitShakeDuration"] = hitShakeDuration;
	out["hitStopDuration"] = hitStopDuration;
	out["spawnedFromTag"] = spawnedFromTag;
}

void ReflexEnemyComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	isTemplate = in.value("isTemplate", isTemplate);
	maxHp = in.value("maxHp", maxHp);
	hp = in.value("hp", maxHp); // 保存時点のhpではなく、複製時は満タンから始めたいためmaxHpを既定値にする
	hitShakeStrength = in.value("hitShakeStrength", hitShakeStrength);
	hitShakeDuration = in.value("hitShakeDuration", hitShakeDuration);
	hitStopDuration = in.value("hitStopDuration", hitStopDuration);
	spawnedFromTag = in.value("spawnedFromTag", spawnedFromTag);
	pendingDestroy = false;
	pendingHitParticle = false;
	pendingHitSound = false;
}

REGISTER_SIMPLE_COMPONENT(ReflexEnemyComponent, "ReflexEnemy", "REFLEX敵", "物理");
