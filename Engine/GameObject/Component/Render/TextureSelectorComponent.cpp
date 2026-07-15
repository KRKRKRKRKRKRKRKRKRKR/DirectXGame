#include "TextureSelectorComponent.h"
#include "RenderComponentBase.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void TextureSelectorComponent::DrawImGui(const char* namePrefix) {
	std::string label = std::string(namePrefix) + "テクスチャ";

	if (ImGui::BeginCombo(label.c_str(), (*textures_)[index_].name.c_str())) {
		for (int i = 0; i < static_cast<int>(textures_->size()); i++) {
			bool selected = (i == index_);
			if (ImGui::Selectable((*textures_)[i].name.c_str(), selected))
				index_ = i;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	target_->textureHandle = (*textures_)[index_].handle;
}
