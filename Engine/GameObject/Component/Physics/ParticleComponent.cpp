#include "ParticleComponent.h"
#include "../../../../Math/EaseUtil.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void ParticleComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)ctx;
	if (!enabled || pendingDestroy) return;

	transform.translation = transform.translation + direction * (speed * deltaTime);

	elapsed += deltaTime;
	if (elapsed >= lifeTime) {
		pendingDestroy = true;
		return;
	}

	float t = EaseUtil::Clamp01(elapsed / lifeTime);
	float easedT = Easing::Apply(sizeEasing, t);
	float size = EaseUtil::Lerp(sizeStart, sizeEnd, easedT);
	transform.scale = { size, size, size };
}

void ParticleComponent::DrawImGui(const char* namePrefix) {
	// 実行時に自動生成される一時オブジェクトのため、Inspectorには経過時間だけ
	// 参考表示する（値の編集はさせない。テンプレート側の設定はParticleEmitterComponentで行う）
	std::string label = std::string(namePrefix) + "パーティクル経過時間";
	ImGui::BeginDisabled();
	ImGui::DragFloat(label.c_str(), &elapsed, 0.0f, 0.0f, lifeTime);
	ImGui::EndDisabled();
}
