#include "ModelRenderComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void ModelRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	if (hasAnimation) {
		renderer->UpdateModelAnimation(modelHandle, deltaTime);
	}

	// サブメッシュごとのindexを実際のTextureHandleへ変換する（未選択/範囲外はkTextureNone＝白）
	std::vector<TextureHandle> subMeshTextures;
	subMeshTextures.reserve(subMeshTextureIndices.size());
	for (int idx : subMeshTextureIndices) {
		if (textures_ && idx >= 0 && idx < static_cast<int>(textures_->size())) {
			subMeshTextures.push_back((*textures_)[idx].handle);
		} else {
			subMeshTextures.push_back(kTextureNone);
		}
	}

	renderer->DrawModel(modelHandle, transform, color, subMeshTextures, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}

void ModelRenderComponent::DrawImGui(const char* namePrefix) {
	RenderComponentBase::DrawImGui(namePrefix);

	if (!rendererForUi_) return; // 生成直後でSetInspectorContext未実行の場合は何もしない

	// 実際のサブメッシュ数に合わせて自動リサイズする（新規追加分は-1=未選択で埋める）
	size_t subMeshCount = rendererForUi_->GetModelSubMeshCount(modelHandle);
	if (subMeshTextureIndices.size() != subMeshCount) {
		subMeshTextureIndices.resize(subMeshCount, -1);
	}

	std::string countLabel = std::string(namePrefix) + "サブメッシュ数: " + std::to_string(subMeshCount);
	ImGui::Text("%s", countLabel.c_str());
	ImGui::TextDisabled("(プロジェクトパネルの画像をドラッグ&ドロップでも割り当てられます)");

	for (size_t s = 0; s < subMeshCount; ++s) {
		std::string comboId = std::string(namePrefix) + "サブメッシュ " + std::to_string(s) + " のテクスチャ";
		int& idx = subMeshTextureIndices[s];
		std::string currentName = (textures_ && idx >= 0 && idx < static_cast<int>(textures_->size()))
			? (*textures_)[idx].name : "(未選択)";

		// textures_が空でもコンボ自体は出す（中身が無いだけ）。ドラッグ&ドロップだけで
		// 最初の1枚を割り当てられるようにするため、ここでは早期returnしない
		if (ImGui::BeginCombo(comboId.c_str(), currentName.c_str())) {
			bool noneSelected = (idx < 0);
			if (ImGui::Selectable("(未選択)", noneSelected)) idx = -1;
			if (noneSelected) ImGui::SetItemDefaultFocus();
			if (textures_) {
				for (int i = 0; i < static_cast<int>(textures_->size()); ++i) {
					bool selected = (idx == i);
					if (ImGui::Selectable((*textures_)[i].name.c_str(), selected)) idx = i;
					if (selected) ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// プロジェクトパネルからの画像ドラッグ&ドロップ受付：直前に描いたコンボへドロップすると
		// そのサブメッシュへ割り当てる。まだ共有テクスチャ一覧に無い画像はensureTextureRegistered_
		// （実体はSceneBase::EnsureTextureRegistered）で先に登録してから、名前でindexを引き直す
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectImageDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(payload->Data));
				if (ensureTextureRegistered_) ensureTextureRegistered_(path);
				std::string droppedName = path.substr(path.find_last_of('/') + 1);
				if (textures_) {
					for (int i = 0; i < static_cast<int>(textures_->size()); ++i) {
						if ((*textures_)[i].name == droppedName) { idx = i; break; }
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
}
