// 10DaysJam
#include "TextSpriteComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../Utils/Logger.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include <cstring>
#include <format>
#include <string>

namespace {

// UTF-8バイト列をUnicodeコードポイント列へデコードする（1〜4バイトシーケンス対応の簡易実装）。
// 不正なバイト列は1バイトずつ読み飛ばす（TextRenderComponent::Utf8Decodeと同じロジック）
std::vector<char32_t> Utf8Decode(const std::string& utf8) {
	std::vector<char32_t> result;
	size_t i = 0;
	while (i < utf8.size()) {
		unsigned char c0 = static_cast<unsigned char>(utf8[i]);
		char32_t cp = 0;
		size_t len = 0;
		if ((c0 & 0x80) == 0x00) { cp = c0; len = 1; }
		else if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; len = 2; }
		else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; len = 3; }
		else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; len = 4; }
		else { ++i; continue; } // 不正な先頭バイトは読み飛ばす

		if (i + len > utf8.size()) break;

		bool valid = true;
		for (size_t k = 1; k < len; ++k) {
			unsigned char ck = static_cast<unsigned char>(utf8[i + k]);
			if ((ck & 0xC0) != 0x80) { valid = false; break; }
			cp = (cp << 6) | (ck & 0x3F);
		}
		if (!valid) { ++i; continue; }

		if (cp != U'\r') result.push_back(cp); // CRLFのCRは無視してLFだけ改行として扱う
		i += len;
	}
	return result;
}

template<size_t N>
void CopyToEditBuffer(char (&buf)[N], const std::string& src) {
	strncpy_s(buf, src.c_str(), N - 1);
}

} // namespace

TextSpriteComponent::TextSpriteComponent() {
	lighting = false;                // スクリーン空間UIなのでライティング不要
	blendMode = BlendMode::kNormal;  // 文字の輪郭を滑らかに見せるためアルファブレンド必須
}

void TextSpriteComponent::Rebuild(Renderer* renderer) {
	renderer_ = renderer;
	if (!renderer) return;

	if (text.empty()) {
		textureHandle = kTextureNone;
		boxWidth = 0.0f;
		boxHeight = 0.0f;
		lastBitmapWidth_ = 0;
		lastBitmapHeight_ = 0;
		return;
	}

	if (!builder_.LoadFont(fontFilePath)) {
		Logger::Log(std::format("TextSpriteComponent::Rebuild: failed to load font '{}'\n", fontFilePath));
		return;
	}

	std::vector<char32_t> decoded = Utf8Decode(text);
	if (decoded.empty()) return;

	TextBitmap bitmap;
	if (!builder_.Build(decoded, fontSize, lineSpacing, bitmap)) {
		Logger::Log("TextSpriteComponent::Rebuild: failed to build text bitmap\n");
		return;
	}

	// Build()は文字列にちょうど収まるサイズのビットマップを返すため、そのままtextureHandle・
	// 箱サイズへ使うだけで「自動スナップ」（文字が増減しても箱が実寸に追従する）になる。
	// 既にテクスチャを持っていて、かつ直前のビットマップと同サイズなら、TextureManagerの
	// ハンドルを新規消費せずUpdateTextureFromPixelsで中身だけ差し替える（GridPuzzleScene::
	// UpdateCostTextのように値が変わるたびに呼ばれるケースで、TextureManager::kMaxTextureCount
	// にすぐ到達してしまうのを防ぐため）。サイズが変わった・初回の場合は新規作成する
	bool canReuse = textureHandle != kTextureNone
		&& bitmap.width == lastBitmapWidth_
		&& bitmap.height == lastBitmapHeight_
		&& renderer->UpdateTextureFromPixels(textureHandle, bitmap.width, bitmap.height, bitmap.rgbaPixels.data());
	if (!canReuse) {
		textureHandle = renderer->CreateTextureFromPixels(bitmap.width, bitmap.height, bitmap.rgbaPixels.data());
	}
	lastBitmapWidth_ = bitmap.width;
	lastBitmapHeight_ = bitmap.height;
	boxWidth = static_cast<float>(bitmap.width);
	boxHeight = static_cast<float>(bitmap.height);
}

void TextSpriteComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	if (textureHandle == kTextureNone) return;
	// assignedNumberKeyが割り当てられている間だけisVisibleを見る（キー未割当の既存の使い方は
	// 従来通り常に表示され続ける。SceneBase::UpdateTextSpriteVisibilityTogglesが対応する
	// 数字キーを押すたびにisVisibleを反転させる）
	if (assignedNumberKey >= 0 && !isVisible) return;

	Transform drawTransform = transform;
	drawTransform.scale = { boxWidth, boxHeight, 1.0f };

	// Sprite2Dのクアッド頂点は中心原点(-0.5〜0.5)固定のため、drawTransform.translationは常に
	// 「テキストの中心」を指す。kLeft/kRightはGameObject本体の位置（translation、Gizmoで
	// 動かす対象）は変えずに、描画用に一時的にtranslation.xだけ箱の半幅分オフセットして実現する
	if (horizontalAlign == HorizontalAlign::kLeft) {
		drawTransform.translation.x += boxWidth * 0.5f;
	} else if (horizontalAlign == HorizontalAlign::kRight) {
		drawTransform.translation.x -= boxWidth * 0.5f;
	}

	renderer->DrawSprite2D(drawTransform, color, textureHandle, lighting, UVTransform{}, blendMode, blendStrength, alphaTest, alphaThreshold);
}

void TextSpriteComponent::DrawImGui(const char* namePrefix) {
	RenderComponentBase::DrawImGui(namePrefix);

	std::string fontSizeLabel = std::string(namePrefix) + "フォントサイズ";
	ImGui::DragFloat(fontSizeLabel.c_str(), &fontSize, 1.0f, 8.0f, 256.0f);

	{
		static const char* kAlignLabels[] = { "左揃え", "中央揃え", "右揃え" };
		int alignIndex = static_cast<int>(horizontalAlign);
		std::string alignLabel = std::string(namePrefix) + "揃え";
		if (ImGui::Combo(alignLabel.c_str(), &alignIndex, kAlignLabels, 3)) {
			horizontalAlign = static_cast<HorizontalAlign>(alignIndex);
		}
	}

	{
		// 表示中の選択肢は"割り当てなし","0".."9"の11個。assignedNumberKey(-1〜9)を+1した値を
		// そのままコンボのインデックスとして使う
		static const char* kKeyLabels[] = { "割り当てなし", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
		int keyIndex = assignedNumberKey + 1;
		std::string keyLabel = std::string(namePrefix) + "表示トグル用の数字キー";
		if (ImGui::Combo(keyLabel.c_str(), &keyIndex, kKeyLabels, 11)) {
			int newKey = keyIndex - 1;
			// 割り当てなし→いずれかの数字キーへ新規に割り当てた瞬間は、まず非表示から始める
			// （「起動直後は全部非表示」という既定の運用に合わせる。既に割り当て済みのキーを
			// 別の数字へ変更する場合はisVisibleの現在値をそのまま維持する）
			if (assignedNumberKey < 0 && newKey >= 0) isVisible = false;
			assignedNumberKey = newKey;
		}
		if (assignedNumberKey >= 0) {
			ImGui::SameLine();
			ImGui::TextDisabled(isVisible ? "(表示中)" : "(非表示)");
		}
	}

	// 初回だけtext（保存済みの確定文字列）をeditBuffer_へ読み込む。以降は「編集」ボタンで
	// 明示的に読み込み直すまで、Inspectorを開き直してもeditBuffer_の内容を保つ
	if (!editBufferInitialized_) {
		CopyToEditBuffer(editBuffer_, text);
		editBufferInitialized_ = true;
	}

	std::string editBoxLabel = std::string(namePrefix) + "テキスト";
	ImGui::InputTextMultiline(editBoxLabel.c_str(), editBuffer_, sizeof(editBuffer_), ImVec2(0.0f, 80.0f));

	std::string editButtonLabel = std::string(namePrefix) + "編集";
	std::string saveButtonLabel = std::string(namePrefix) + "保存";
	std::string deleteButtonLabel = std::string(namePrefix) + "削除";

	// 編集：確定済みtextを（他所で書き換わっていた場合に備え）editBuffer_へ読み込み直すだけ
	if (ImGui::Button(editButtonLabel.c_str())) {
		CopyToEditBuffer(editBuffer_, text);
	}
	ImGui::SameLine();
	// 保存：editBuffer_の内容をtextへ確定し、フォントサイズ等の現在値も合わせてRebuild()する
	if (ImGui::Button(saveButtonLabel.c_str())) {
		text = editBuffer_;
		if (renderer_) Rebuild(renderer_);
	}
	ImGui::SameLine();
	// 削除：textを空にして再構築する（textureHandleがkTextureNoneに戻り、何も描画されなくなる）
	if (ImGui::Button(deleteButtonLabel.c_str())) {
		text.clear();
		editBuffer_[0] = '\0';
		if (renderer_) Rebuild(renderer_);
	}

	std::string sizeInfo = std::string(namePrefix) + "箱サイズ: "
		+ std::to_string(static_cast<int>(boxWidth)) + "x" + std::to_string(static_cast<int>(boxHeight)) + "px";
	ImGui::Text("%s", sizeInfo.c_str());
}

void TextSpriteComponent::ToJson(nlohmann::json& out) const {
	RenderComponentBase::ToJson(out);
	out["text"] = text;
	out["fontSize"] = fontSize;
	out["lineSpacing"] = lineSpacing;
	out["fontFilePath"] = fontFilePath;
	out["horizontalAlign"] = static_cast<int>(horizontalAlign);
	out["assignedNumberKey"] = assignedNumberKey;
	out["isVisible"] = isVisible;
}

void TextSpriteComponent::FromJson(const nlohmann::json& in) {
	RenderComponentBase::FromJson(in);
	text = in.value("text", text);
	fontSize = in.value("fontSize", fontSize);
	lineSpacing = in.value("lineSpacing", lineSpacing);
	fontFilePath = in.value("fontFilePath", fontFilePath);
	horizontalAlign = static_cast<HorizontalAlign>(in.value("horizontalAlign", static_cast<int>(horizontalAlign)));
	assignedNumberKey = in.value("assignedNumberKey", assignedNumberKey);
	isVisible = in.value("isVisible", isVisible);
	editBufferInitialized_ = false; // 次のDrawImGuiでeditBuffer_をtextから作り直す
}

// このコンポーネントはAddComponent直後にRebuild(renderer)を呼ぶ必要があるため、
// REGISTER_SIMPLE_COMPONENTは使わず、Engine/GameObject/ComponentRegistration.cppで
// ComponentRegistry::Register<TextSpriteComponent>を手書きしている（ModelRenderComponent/
// SpriteRenderComponentと同じ理由）
