#include "ParticleTemplateComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void ParticleTemplateComponent::DrawImGui(const char* namePrefix) {
	ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "%s",
		(std::string(namePrefix) + "パーティクルテンプレート（Play開始時に自動で隠されます）").c_str());
	ImGui::TextWrapped("%s", (std::string(namePrefix)
		+ "このタグ名をコードから指定すると、見た目とParticleEmitterComponentの発生設定が複製されます。").c_str());
}

REGISTER_SIMPLE_COMPONENT(ParticleTemplateComponent, "ParticleTemplate", "パーティクルテンプレート", "物理");
