#include "TextRenderComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../Utils/StringUtils.h"
#include "../../../Utils/Logger.h"
#include <fstream>
#include <sstream>
#include <string>
#include <format>

namespace {

// UTF-8バイト列をUnicodeコードポイント列へデコードする（1〜4バイトシーケンス対応の簡易実装）。
// 不正なバイト列は1バイトずつ読み飛ばす
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

} // namespace

TextRenderComponent::TextRenderComponent() {
	lighting = false;         // Sprite2Dと同じくスクリーン空間UIなのでライティング不要
	blendMode = BlendMode::kNormal; // 文字の輪郭を滑らかに見せるためアルファブレンド必須
}

namespace {

// Sprite2Dと同じ「ピクセル座標・左上原点」のスクリーン空間UIとしてTransformComponentを設定する
// （CreateStatic/CreateDynamic共通の下ごしらえ）
void SetupScreenSpaceTransform(GameObject& obj) {
	TransformComponent* transform = obj.GetComponent<TransformComponent>();
	transform->is2D = true;
	transform->translationSpeed = 1.0f; transform->translationMin = 0.0f; transform->translationMax = 1920.0f;
	transform->scaleSpeed = 1.0f; transform->scaleMin = 1.0f; transform->scaleMax = 1920.0f;
}

} // namespace

TextRenderComponent* TextRenderComponent::CreateStatic(GameObject& obj, Renderer* renderer,
	const std::string& txtPath, const std::string& fontPath, float fontSize) {
	SetupScreenSpaceTransform(obj);

	TextRenderComponent* textRender = obj.AddComponent<TextRenderComponent>();
	textRender->txtFilePath = txtPath;
	textRender->fontFilePath = fontPath;
	textRender->fontSize = fontSize;
	textRender->Load(renderer);

	// 箱(Transform.scale)の初期値は表示上の基準サイズに合わせる。txt/フォントが見つからず
	// Load()が失敗した場合はビットマップ実寸が0のままなのでscaleは{0,0,1}になる（見えないだけ）
	obj.GetTransform().scale = { textRender->GetNativeWidth(), textRender->GetNativeHeight(), 1.0f };
	return textRender;
}

TextRenderComponent* TextRenderComponent::CreateDynamic(GameObject& obj, Renderer* renderer,
	const std::string& fontPath, float fontSize, uint32_t canvasWidth, uint32_t canvasHeight) {
	SetupScreenSpaceTransform(obj);

	TextRenderComponent* textRender = obj.AddComponent<TextRenderComponent>();
	textRender->fontFilePath = fontPath;
	textRender->fontSize = fontSize;
	textRender->canvasWidth = canvasWidth;
	textRender->canvasHeight = canvasHeight;
	textRender->LoadDynamic(renderer);

	obj.GetTransform().scale = { textRender->GetNativeWidth(), textRender->GetNativeHeight(), 1.0f };
	return textRender;
}

bool TextRenderComponent::Load(Renderer* renderer) {
	renderer_ = renderer;
	bitmapWidth_ = 0;
	bitmapHeight_ = 0;
	textureHandle = kTextureNone;

	// txtFilePathはUTF-8前提（Resources/Text/{name}.txtの{name}はImGuiのInputText由来）。
	// std::string版のifstreamコンストラクタに直接渡すと実行時ロケールでパスが解釈され、
	// 日本語ファイル名が正しく開けなくなるため、wstringへ変換してから渡す
	std::ifstream txtFile(StringUtils::ConvertString(txtFilePath), std::ios::binary);
	if (!txtFile.is_open()) {
		Logger::Log(std::format("TextRenderComponent::Load: failed to open txt file '{}'\n", txtFilePath));
		return false;
	}
	std::ostringstream ss;
	ss << txtFile.rdbuf();
	std::vector<char32_t> text = Utf8Decode(ss.str());
	if (text.empty()) {
		Logger::Log(std::format("TextRenderComponent::Load: '{}' is empty after UTF-8 decode\n", txtFilePath));
		return false;
	}

	if (!builder_.LoadFont(fontFilePath)) {
		Logger::Log(std::format("TextRenderComponent::Load: failed to load font '{}'\n", fontFilePath));
		return false;
	}

	// superSample倍だけ大きく焼いておくことで、箱(Transform.scale)をsuperSample倍まで
	// 拡大してもビットマップの実解像度を超えずぼやけない
	TextBitmap bitmap;
	if (!builder_.Build(text, fontSize * superSample, lineSpacing, bitmap)) {
		Logger::Log(std::format("TextRenderComponent::Load: failed to build text bitmap for '{}'\n", txtFilePath));
		return false;
	}

	textureHandle = renderer->CreateTextureFromPixels(bitmap.width, bitmap.height, bitmap.rgbaPixels.data());
	bitmapWidth_ = bitmap.width;
	bitmapHeight_ = bitmap.height;
	return true;
}

bool TextRenderComponent::LoadDynamic(Renderer* renderer) {
	renderer_ = renderer;
	dynamicText = true;
	textureHandle = kTextureNone;

	if (!builder_.LoadFont(fontFilePath)) {
		Logger::Log(std::format("TextRenderComponent::LoadDynamic: failed to load font '{}'\n", fontFilePath));
		return false;
	}

	// 最初は空文字列で固定サイズのテクスチャを1回だけ確保する。以降はSetText()が
	// このテクスチャの中身だけを書き換える（ハンドルは変わらない）
	TextBitmap bitmap;
	if (!builder_.BuildFixed({}, fontSize * superSample, lineSpacing, canvasWidth, canvasHeight, bitmap)) {
		Logger::Log(std::format("TextRenderComponent::LoadDynamic: BuildFixed failed (canvas {}x{})\n", canvasWidth, canvasHeight));
		return false;
	}

	textureHandle = renderer->CreateTextureFromPixels(bitmap.width, bitmap.height, bitmap.rgbaPixels.data());
	bitmapWidth_ = bitmap.width;
	bitmapHeight_ = bitmap.height;
	return true;
}

bool TextRenderComponent::SetText(Renderer* renderer, const std::string& utf8Text) {
	if (textureHandle == kTextureNone) return false; // LoadDynamic()が未実行・失敗している

	std::vector<char32_t> text = Utf8Decode(utf8Text);
	TextBitmap bitmap;
	if (!builder_.BuildFixed(text, fontSize * superSample, lineSpacing, canvasWidth, canvasHeight, bitmap)) return false;

	return renderer->UpdateTextureFromPixels(textureHandle, bitmap.width, bitmap.height, bitmap.rgbaPixels.data());
}

bool TextRenderComponent::UpdateDynamicText(Renderer* renderer) {
	if (!dynamicText || !textProvider_) return false;
	return SetText(renderer, textProvider_());
}

void TextRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	// TODO(debug): テキスト非表示バグ調査用の一時ログ。原因特定後に削除する（インスタンスごとに
	// カウントするため、各テキストが必ず一定間隔でログに出る＝呼ばれていないテキストは
	// このログ自体が一切出なくなる、という判定ができる）
	if (++debugDrawCounter_ % 120 == 1) {
		Logger::Log(std::format("TextRenderComponent::Draw '{}' textureHandle={} dynamicText={} pos=({:.1f},{:.1f})\n",
			txtFilePath.empty() ? "(dynamic)" : txtFilePath, textureHandle, dynamicText, transform.translation.x, transform.translation.y));
	}
	if (textureHandle == kTextureNone) return; // Load失敗・未Loadなら何も描画しない

	// 箱の大きさ(transform.scale、Gizmoで自由に変更できる)とフォントサイズ(ラスタライズ解像度)は
	// 独立させる。SpriteRenderComponentと同じくtransformをそのまま使い、テクスチャは箱に合わせて
	// 伸縮する（fontSizeを上げると同じ箱の中で文字が高解像度になるだけで、箱自体は変わらない）
	renderer->DrawSprite2D(transform, color, textureHandle, lighting, UVTransform{}, blendMode, blendStrength, alphaTest, alphaThreshold);
}

void TextRenderComponent::DrawImGui(const char* namePrefix) {
	RenderComponentBase::DrawImGui(namePrefix);

	// fontSize/lineSpacingは合成ビットマップに焼き込み済みの値なので、変更しただけでは見た目に
	// 反映されない。静的テキストは値を編集した後「Reload Text」でLoad()を呼び直す必要がある
	// （毎フレーム自動リロードするとTextureManagerのハンドルを再生成のたびに消費し続けて
	// kMaxTextureCountに達してしまうため、明示的なボタン操作にしている）。
	// dynamicTextはUpdateDynamicText()が毎フレームSetText()（=BuildFixedで焼き直し）を呼ぶため、
	// 値を変えるだけで次のフレームから自動的に反映される（Reloadボタンは不要）。
	// 箱の大きさ（画面上に見える大きさ）はTransform.scale側（Gizmo/Objectsパネルで操作）が
	// 担当し、fontSizeはあくまで文字のラスタライズ解像度（＝くっきり度）。箱サイズは変わらない
	std::string fontSizeLabel = std::string(namePrefix) + "フォントサイズ";
	std::string lineSpacingLabel = std::string(namePrefix) + "行間";
	std::string superSampleLabel = std::string(namePrefix) + "スーパーサンプリング（にじみ余白）";
	ImGui::DragFloat(fontSizeLabel.c_str(), &fontSize, 1.0f, 8.0f, 256.0f);
	ImGui::DragFloat(lineSpacingLabel.c_str(), &lineSpacing, 0.05f, 0.5f, 3.0f);
	ImGui::DragFloat(superSampleLabel.c_str(), &superSample, 0.1f, 1.0f, 4.0f);

	std::string source = dynamicText ? std::string("（動的、SetText()で更新）") : ("テキスト: " + txtFilePath);
	std::string info = std::string(namePrefix) + " " + source + " / フォント: " + fontFilePath +
		" （焼き込み " + std::to_string(bitmapWidth_) + "x" + std::to_string(bitmapHeight_) + "px, 実寸 " +
		std::to_string(static_cast<int>(GetNativeWidth())) + "x" + std::to_string(static_cast<int>(GetNativeHeight())) + "px）";
	ImGui::Text("%s", info.c_str());

	// dynamicTextは毎フレームSetText()が呼ばれる想定のため、Reloadボタン（静的txt読み込み用）は出さない
	if (!dynamicText) {
		std::string reloadLabel = std::string(namePrefix) + "テキストを再読み込み";
		if (renderer_ && ImGui::Button(reloadLabel.c_str())) {
			Load(renderer_);
		}
	}
}

void TextRenderComponent::ToJson(nlohmann::json& out) const {
	RenderComponentBase::ToJson(out);
	out["txtFilePath"] = txtFilePath;
	out["fontFilePath"] = fontFilePath;
	out["fontSize"] = fontSize;
	out["lineSpacing"] = lineSpacing;
	out["superSample"] = superSample;
	out["dynamicText"] = dynamicText;
	out["canvasWidth"] = canvasWidth;
	out["canvasHeight"] = canvasHeight;
	out["hudKey"] = hudKey;
}

void TextRenderComponent::FromJson(const nlohmann::json& in) {
	RenderComponentBase::FromJson(in);
	txtFilePath = in.value("txtFilePath", txtFilePath);
	fontFilePath = in.value("fontFilePath", fontFilePath);
	fontSize = in.value("fontSize", fontSize);
	lineSpacing = in.value("lineSpacing", lineSpacing);
	superSample = in.value("superSample", superSample);
	dynamicText = in.value("dynamicText", dynamicText);
	canvasWidth = in.value("canvasWidth", canvasWidth);
	canvasHeight = in.value("canvasHeight", canvasHeight);
	hudKey = in.value("hudKey", hudKey);
}
