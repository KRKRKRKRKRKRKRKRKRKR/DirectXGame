#include "AlphabetTextComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/EasingPreview.h"
#include <cstring>

void AlphabetTextComponent::DrawImGui(const char* namePrefix) {
	// ImGui::InputTextは素のchar[]バッファを要求するため、textとの相互変換をここで行う
	// （SceneBase.cppのstaticTextContentBufと同じ固定サイズバッファ方式）
	char buf[256];
	strncpy_s(buf, text.c_str(), sizeof(buf) - 1);

	std::string textLabel = std::string(namePrefix) + "文字列（A-Z・0-9・半角スペースのみ）";
	std::string scaleLabel = std::string(namePrefix) + "文字サイズ";
	std::string spacingLabel = std::string(namePrefix) + "文字間隔";
	std::string spaceWidthLabel = std::string(namePrefix) + "スペースの幅";

	// textProviderが設定されている間は毎フレームtextが上書きされるため、手打ち編集を許すと
	// 次のフレームで即座に消えてしまい紛らわしい。無効化して「動的更新中」であることを示す
	bool hasProvider = static_cast<bool>(textProvider_);
	if (hasProvider) ImGui::BeginDisabled();
	if (ImGui::InputText(textLabel.c_str(), buf, sizeof(buf))) {
		text = buf;
	}
	if (hasProvider) ImGui::EndDisabled();
	if (hasProvider) {
		ImGui::TextDisabled("(SetTextProviderにより毎フレーム自動更新中)");
	}

	ImGui::DragFloat(scaleLabel.c_str(), &charScale, 0.05f, 0.05f, 20.0f);
	ImGui::DragFloat(spacingLabel.c_str(), &charSpacing, 0.05f, 0.05f, 20.0f);
	ImGui::DragFloat(spaceWidthLabel.c_str(), &spaceWidth, 0.05f, 0.0f, 20.0f);
	ImGui::TextDisabled("(対応する.objが無い文字は表示されません。読み込み元: Resources/Alphabet/)");

	std::string alignLabel = std::string(namePrefix) + "揃え";
	static const char* kAlignNames[] = { "左揃え", "中央揃え", "右揃え" };
	int alignIndex = static_cast<int>(horizontalAlign);
	if (ImGui::Combo(alignLabel.c_str(), &alignIndex, kAlignNames, 3)) {
		horizontalAlign = static_cast<HorizontalAlign>(alignIndex);
	}

	ImGui::Separator();
	std::string entranceLabel = std::string(namePrefix) + "1文字ずつ登場演出";
	if (ImGui::Checkbox(entranceLabel.c_str(), &useCharEntranceAnimation)) {
		// チェックのON/OFF自体はtext/charScale/charSpacing/spaceWidthのどれとも異なるため、
		// このままではlastBuiltText等と食い違いが生じず子GameObjectが作り直されない。
		// lastBuiltTextを強制的に不一致にして、次のUpdateAlphabetTextComponentsで
		// 確実にRebuildAlphabetTextChildrenが呼ばれるようにする
		lastBuiltText.clear();
	}
	if (useCharEntranceAnimation) {
		std::string delayLabel = std::string(namePrefix) + "1文字あたりの遅延(秒)";
		std::string zOffsetLabel = std::string(namePrefix) + "登場Zオフセット";
		std::string durationLabel = std::string(namePrefix) + "登場時間(秒)";
		std::string easingLabel = std::string(namePrefix) + "登場イージング";
		ImGui::DragFloat(delayLabel.c_str(), &entranceCharDelay, 0.005f, 0.0f, 1.0f);
		ImGui::DragFloat(zOffsetLabel.c_str(), &entranceZOffset, 0.1f, -100.0f, 100.0f);
		ImGui::DragFloat(durationLabel.c_str(), &entranceDuration, 0.02f, 0.02f, 5.0f);

		int easingIndex = static_cast<int>(entranceEasing);
		const char* const* easingNames = Easing::GetTypeNames();
		if (ImGui::BeginCombo(easingLabel.c_str(), easingNames[easingIndex])) {
			for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
				bool selected = (i == easingIndex);
				if (ImGui::Selectable(easingNames[i], selected)) entranceEasing = static_cast<Easing::Type>(i);
				if (selected) ImGui::SetItemDefaultFocus();
				EasingPreview::ShowOnHover(static_cast<Easing::Type>(i));
			}
			ImGui::EndCombo();
		}
	}
}

void AlphabetTextComponent::ToJson(nlohmann::json& out) const {
	out["text"] = text;
	out["charScale"] = charScale;
	out["charSpacing"] = charSpacing;
	out["spaceWidth"] = spaceWidth;
	out["horizontalAlign"] = static_cast<int>(horizontalAlign);
	out["useCharEntranceAnimation"] = useCharEntranceAnimation;
	out["entranceCharDelay"] = entranceCharDelay;
	out["entranceZOffset"] = entranceZOffset;
	out["entranceDuration"] = entranceDuration;
	out["entranceEasing"] = static_cast<int>(entranceEasing);
}

void AlphabetTextComponent::FromJson(const nlohmann::json& in) {
	text = in.value("text", text);
	charScale = in.value("charScale", charScale);
	charSpacing = in.value("charSpacing", charSpacing);
	// spaceWidthは新規追加パラメータ。既存の保存データ（spaceWidthキーが無い）を読み込む場合は
	// charSpacingと同じ値にフォールバックする（従来通り「スペースも通常文字と同じ間隔」という
	// 見た目を保つため）
	spaceWidth = in.value("spaceWidth", charSpacing);
	horizontalAlign = static_cast<HorizontalAlign>(in.value("horizontalAlign", static_cast<int>(horizontalAlign)));
	useCharEntranceAnimation = in.value("useCharEntranceAnimation", useCharEntranceAnimation);
	entranceCharDelay = in.value("entranceCharDelay", entranceCharDelay);
	entranceZOffset = in.value("entranceZOffset", entranceZOffset);
	entranceDuration = in.value("entranceDuration", entranceDuration);
	entranceEasing = static_cast<Easing::Type>(in.value("entranceEasing", static_cast<int>(entranceEasing)));
	// lastBuiltTextはあえて復元しない（空のままにしておくことで、Load直後の最初の
	// UpdateAlphabetTextComponentsで必ず子GameObjectが作り直される。子GameObject自体は
	// SceneBase::LoadSceneが読み込み直後にClearAlphabetTextChildrenで一旦消しているため、
	// textと突き合わせて毎回作り直す必要がある）
}

namespace {
// RegisterSimple（REGISTER_SIMPLE_COMPONENTマクロ）ではなくComponentRegistry::Registerを直接
// 使うのは、RegisterSimpleがGetSimpleTypeNames()（Add Componentメニューの自動一覧、"描画"カテゴリの
// 見出しを自動生成する）にも登録してしまうため。SceneBase::DrawAddComponentMenuは「描画」の
// 見出しを個別UI群（モデル描画/スプライト描画等）用に既に手書きしているため、自動一覧側にも
// 同名の「描画」見出しが重複して出てしまう（紛らわしい）。Register<T>ならJSON保存/削除
// （RemoveByTypeName）の登録だけ行い、Add Componentメニューへの表示はSceneBase::
// DrawAddComponentMenu側の「描画」セクションに手動で1行追加する
struct AlphabetTextComponent_AutoRegister {
	AlphabetTextComponent_AutoRegister() {
		ComponentRegistry::Register<AlphabetTextComponent>("AlphabetText",
			[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
				obj.AddComponent<AlphabetTextComponent>()->FromJson(data);
			},
			[](GameObject& obj) { return obj.RemoveComponent<AlphabetTextComponent>(); },
			"アルファベット文字列");
	}
};
AlphabetTextComponent_AutoRegister g_AlphabetTextComponent_autoRegister;
} // namespace
