// 10DaysJam
#include "TextGroupComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <cstring>

void TextGroupComponent::DrawImGui(const char* namePrefix) {
	std::string prefix(namePrefix);

	// ボタン押下でentriesを書き換えると、その場でループを続けるとイテレータ/インデックスが
	// ズレるため、削除要求だけ控えておいてループを抜けてから処理する
	int removeIndex = -1;

	for (size_t i = 0; i < entries.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		Entry& entry = entries[i];

		// ImGui::InputTextは素のchar[]バッファを要求するため、std::stringとの相互変換をここで行う
		// （AlphabetTextComponent::DrawImGuiと同じ固定サイズバッファ方式）
		char buf[256];
		strncpy_s(buf, entry.text.c_str(), sizeof(buf) - 1);
		std::string textLabel = prefix + "テキスト";
		if (ImGui::InputText(textLabel.c_str(), buf, sizeof(buf))) {
			entry.text = buf;
		}

		std::string fontSizeLabel = prefix + "フォントサイズ";
		ImGui::DragFloat(fontSizeLabel.c_str(), &entry.fontSize, 0.5f, 4.0f, 256.0f);

		std::string alignLabel = prefix + "揃え";
		static const char* kAlignNames[] = { "左揃え", "中央揃え", "右揃え" };
		int alignIndex = static_cast<int>(entry.horizontalAlign);
		if (ImGui::Combo(alignLabel.c_str(), &alignIndex, kAlignNames, 3)) {
			entry.horizontalAlign = static_cast<TextSpriteComponent::HorizontalAlign>(alignIndex);
		}

		std::string colorLabel = prefix + "色";
		ImGui::ColorEdit4(colorLabel.c_str(), &entry.color.x);

		std::string removeLabel = prefix + "削除##RemoveEntry";
		if (ImGui::Button(removeLabel.c_str())) {
			removeIndex = static_cast<int>(i);
		}
		ImGui::Separator();
		ImGui::PopID();
	}

	if (removeIndex >= 0) {
		entries.erase(entries.begin() + removeIndex);
	}

	std::string addLabel = prefix + "エントリを追加";
	if (ImGui::Button(addLabel.c_str())) {
		entries.push_back(Entry{});
	}

	ImGui::Separator();
	std::string anchorLabel = prefix + "基準位置(px、オーナーからの相対)";
	ImGui::DragFloat2(anchorLabel.c_str(), &anchorOffset.x, 1.0f);

	std::string spacingLabel = prefix + "エントリ間隔(px)";
	ImGui::DragFloat(spacingLabel.c_str(), &spacing, 1.0f, 0.0f, 2000.0f);

	std::string directionLabel = prefix + "並べる方向";
	static const char* kDirectionNames[] = { "縦", "横" };
	int directionIndex = static_cast<int>(stackDirection);
	if (ImGui::Combo(directionLabel.c_str(), &directionIndex, kDirectionNames, 2)) {
		stackDirection = static_cast<StackDirection>(directionIndex);
	}
}

void TextGroupComponent::ToJson(nlohmann::json& out) const {
	nlohmann::json entriesJson = nlohmann::json::array();
	for (const Entry& entry : entries) {
		nlohmann::json entryJson;
		entryJson["text"] = entry.text;
		entryJson["fontSize"] = entry.fontSize;
		entryJson["horizontalAlign"] = static_cast<int>(entry.horizontalAlign);
		entryJson["color"] = Vector4ToJson(entry.color);
		entriesJson.push_back(entryJson);
	}
	out["entries"] = entriesJson;
	out["anchorOffset"] = Vector3ToJson(anchorOffset);
	out["spacing"] = spacing;
	out["stackDirection"] = static_cast<int>(stackDirection);
}

void TextGroupComponent::FromJson(const nlohmann::json& in) {
	entries.clear();
	if (in.contains("entries")) {
		for (const auto& entryJson : in["entries"]) {
			Entry entry;
			entry.text = entryJson.value("text", std::string());
			entry.fontSize = entryJson.value("fontSize", 32.0f);
			entry.horizontalAlign = static_cast<TextSpriteComponent::HorizontalAlign>(
				entryJson.value("horizontalAlign", static_cast<int>(TextSpriteComponent::HorizontalAlign::kCenter)));
			if (entryJson.contains("color")) entry.color = Vector4FromJson(entryJson["color"]);
			entries.push_back(entry);
		}
	}
	if (in.contains("anchorOffset")) anchorOffset = Vector3FromJson(in["anchorOffset"]);
	spacing = in.value("spacing", spacing);
	stackDirection = static_cast<StackDirection>(in.value("stackDirection", static_cast<int>(stackDirection)));
	// builtOnceはあえて復元しない（falseのままにしておくことで、Load直後の最初の
	// UpdateTextGroupComponentsで必ず子GameObjectが作り直される。子GameObject自体は
	// excludeFromSave=trueのためロード直後は存在しない）
}

namespace {
// RegisterSimple（REGISTER_SIMPLE_COMPONENTマクロ）ではなくComponentRegistry::Registerを直接
// 使う理由はAlphabetTextComponentと同じ：RegisterSimpleはGetSimpleTypeNames()（Add Componentメニューの
// 自動一覧、"描画"見出しを自動生成する）にも登録してしまうため。SceneBase::DrawAddComponentMenuは
// 「描画」の見出しを個別UI群として既に手書きしているため、自動一覧側にも同名の「描画」見出しが
// 重複して出てしまう（紛らわしい）
struct TextGroupComponent_AutoRegister {
	TextGroupComponent_AutoRegister() {
		ComponentRegistry::Register<TextGroupComponent>("TextGroup",
			[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
				obj.AddComponent<TextGroupComponent>()->FromJson(data);
			},
			[](GameObject& obj) { return obj.RemoveComponent<TextGroupComponent>(); },
			"テキストグループ");
	}
};
TextGroupComponent_AutoRegister g_TextGroupComponent_autoRegister;
} // namespace
