#include "ComponentManager.h"
#include "GameObject.h"
#include "ComponentRegistry.h"
#include "../../Externals/imgui/imgui.h"
#include <cstdint>

namespace {
	// 実アイコン画像は無いため、型名の文字列ハッシュから決定論的に色を出してアイコン代わりの
	// 色チップにする（同じ型名なら常に同じ色になり、コンポーネントの見分けの助けになる）
	ImVec4 TypeNameToColor(const std::string& typeName) {
		uint32_t hash = 2166136261u; // FNV-1a
		for (char ch : typeName) {
			hash ^= static_cast<uint8_t>(ch);
			hash *= 16777619u;
		}
		float hue = static_cast<float>(hash % 360) / 360.0f;
		float r, g, b;
		ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 0.85f, r, g, b);
		return ImVec4(r, g, b, 1.0f);
	}
}

void ComponentManager::Move(size_t fromIndex, size_t toIndex) {
	if (fromIndex >= components_.size() || toIndex >= components_.size() || fromIndex == toIndex) return;
	auto moved = std::move(components_[fromIndex]);
	components_.erase(components_.begin() + fromIndex);
	components_.insert(components_.begin() + toIndex, std::move(moved));
}

void ComponentManager::DrawImGui(GameObject& owner) {
	for (size_t i = 0; i < components_.size(); i++) {
		IComponent* c = components_[i].get();
		// TransformComponentは常にGameObjectのコンストラクタで先頭(index 0)に追加される前提
		// （Unity同様、Transformは並び替え・削除の対象外にする）
		bool isTransform = (i == 0);
		std::string typeName = isTransform ? "Transform" : ComponentRegistry::GetTypeName(typeid(*c));
		if (typeName.empty()) typeName = "Component";
		// displayNameは見出し等のUI表示専用。typeName自体は保存キー・RemoveByTypeNameの
		// 照合キーとして使うため英語のまま変更しない
		std::string displayName = isTransform ? "トランスフォーム" : ComponentRegistry::GetDisplayName(typeName);

		ImGui::PushID(c);

		ImGui::ColorButton("##icon", TypeNameToColor(typeName),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoDragDrop,
			ImVec2(14, 14));
		ImGui::SameLine();

		// このフレームでcomponents_自体を書き換えたか（Remove/Move/ドラッグドロップ）。
		// trueになったら以降このオブジェクトの残りは描画せず即座にループを打ち切る。
		// 同一フレーム内でコンテナを書き換えた後に古いインデックスを使い続けると、
		// Hierarchyの実装時に踏んだのと同種のクラッシュ（範囲外アクセス/ID対応崩れ）を招くため
		bool structuralChange = false;

		if (!isTransform) {
			ImGui::TextDisabled("::");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("ドラッグで並び替え");
			// TextDisabled()はID無しアイテムのため、そのままBeginDragDropSource()すると
			// 「IDが無い要素からのドラッグ開始」でIM_ASSERT(0)に当たりクラッシュする。
			// ImGuiDragDropFlags_SourceAllowNullIDでID無し要素からのドラッグを明示的に許可する
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				ImGui::SetDragDropPayload("INSPECTOR_COMPONENT_INDEX", &i, sizeof(size_t));
				ImGui::Text("%s", displayName.c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("INSPECTOR_COMPONENT_INDEX")) {
					size_t from = *static_cast<const size_t*>(payload->Data);
					if (from != i && from != 0) {
						Move(from, i);
						structuralChange = true;
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::SameLine();
		}

		bool open = ImGui::CollapsingHeader(displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

		if (!isTransform && !structuralChange && ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("コンポーネントを削除")) {
				ComponentRegistry::RemoveByTypeName(typeName, owner);
				structuralChange = true;
			}
			if (ImGui::MenuItem("上へ移動", nullptr, false, i > 1)) {
				Move(i, i - 1);
				structuralChange = true;
			}
			if (ImGui::MenuItem("下へ移動", nullptr, false, i + 1 < components_.size())) {
				Move(i, i + 1);
				structuralChange = true;
			}
			ImGui::EndPopup();
		}

		if (structuralChange) {
			ImGui::PopID();
			break;
		}

		if (open) {
			ImGui::Indent();
			c->DrawImGui("");
			ImGui::Unindent();
		}
		ImGui::PopID();
		ImGui::Separator();
	}
}
