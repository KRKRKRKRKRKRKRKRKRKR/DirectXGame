#include "TextRenderComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Math/VectorMath.h"
#include "../../../Utils/StringUtils.h"
#include "../../../Utils/Logger.h"
#include <fstream>
#include <sstream>
#include <string>
#include <format>
#include <cstring>

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

// txtFilePathの中身をUTF-8のままstd::stringで読み込む（Utf8Decode前の生バイト列）。
// 日本語ファイル名を正しく開くためwstring変換してから渡すのはLoad()と同じ理由
bool ReadTxtFileRaw(const std::string& txtFilePath, std::string& out) {
	std::ifstream txtFile(StringUtils::ConvertString(txtFilePath), std::ios::binary);
	if (!txtFile.is_open()) return false;
	std::ostringstream ss;
	ss << txtFile.rdbuf();
	out = ss.str();
	return true;
}

// srcをInspectorの固定サイズ編集バッファへコピーする（bufSize超過分は切り詰める。
// マルチバイト文字の途中で切れる可能性はあるが、編集バッファはあくまで表示・入力用であり、
// 元のtxtFilePath自体はLoad()側で全文読み込むため実害はない）
template<size_t N>
void CopyToEditBuffer(char (&buf)[N], const std::string& src) {
	strncpy_s(buf, src.c_str(), N - 1);
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

// 3Dワールド空間のオブジェクトとしてTransformComponentを設定する（is2Dはデフォルトのfalseのまま、
// 他のCube/Sphere等と同じ範囲に揃える）。SpriteRenderComponentのAdd Componentメニューと同じく、
// is2D/is3Dが食い違ったまま保存されるとDrawSprite3D/2Dの座標解釈がズレて描画がおかしくなるため、
// is2Dを明示的にfalseへ揃えておく
void SetupWorldSpaceTransform(GameObject& obj) {
	TransformComponent* transform = obj.GetComponent<TransformComponent>();
	transform->is2D = false;
}

// 焼き込みビットマップの実寸(px)は、3Dワールド空間ではそのままだと巨大すぎる（例:
// 200x50pxの文字なら200x50ユニットになってしまう）。「100px = 1ユニット」という単純な換算で
// 実用的なサイズに縮小する。他のオブジェクトと同じくGizmoのScaleで後から自由に調整できる
constexpr float kWorldSpaceTextPixelsPerUnit = 100.0f;

} // namespace

TextRenderComponent* TextRenderComponent::CreateStatic(GameObject& obj, Renderer* renderer,
	const std::string& txtPath, const std::string& fontPath, float fontSize, bool is3D) {
	if (is3D) SetupWorldSpaceTransform(obj); else SetupScreenSpaceTransform(obj);

	TextRenderComponent* textRender = obj.AddComponent<TextRenderComponent>();
	textRender->is3D = is3D;
	textRender->txtFilePath = txtPath;
	textRender->fontFilePath = fontPath;
	textRender->fontSize = fontSize;
	textRender->Load(renderer);

	// 箱(localScale)の初期値は表示上の基準サイズに合わせる。txt/フォントが見つからず
	// Load()が失敗した場合はビットマップ実寸が0のままなのでlocalScaleは{0,0,1}になる（見えないだけ）。
	// GameObject共有のTransform.scaleではなくlocalScaleに設定するのは、1GameObjectに複数の
	// TextRenderComponentを付けたときに互いのサイズが干渉しないようにするため
	float unitScale = is3D ? (1.0f / kWorldSpaceTextPixelsPerUnit) : 1.0f;
	textRender->localScale = { textRender->GetNativeWidth() * unitScale, textRender->GetNativeHeight() * unitScale, 1.0f };
	return textRender;
}

TextRenderComponent* TextRenderComponent::CreateDynamic(GameObject& obj, Renderer* renderer,
	const std::string& fontPath, float fontSize, uint32_t canvasWidth, uint32_t canvasHeight, bool is3D) {
	if (is3D) SetupWorldSpaceTransform(obj); else SetupScreenSpaceTransform(obj);

	TextRenderComponent* textRender = obj.AddComponent<TextRenderComponent>();
	textRender->is3D = is3D;
	textRender->fontFilePath = fontPath;
	textRender->fontSize = fontSize;
	textRender->canvasWidth = canvasWidth;
	textRender->canvasHeight = canvasHeight;
	textRender->LoadDynamic(renderer);

	float unitScale = is3D ? (1.0f / kWorldSpaceTextPixelsPerUnit) : 1.0f;
	textRender->localScale = { textRender->GetNativeWidth() * unitScale, textRender->GetNativeHeight() * unitScale, 1.0f };
	return textRender;
}

bool TextRenderComponent::Load(Renderer* renderer) {
	renderer_ = renderer;
	bitmapWidth_ = 0;
	bitmapHeight_ = 0;
	textureHandle = kTextureNone;

	// txtFilePathはUTF-8前提（Resources/Text/{name}.txtの{name}はImGuiのInputText由来）
	std::string raw;
	if (!ReadTxtFileRaw(txtFilePath, raw)) {
		Logger::Log(std::format("TextRenderComponent::Load: failed to open txt file '{}'\n", txtFilePath));
		return false;
	}
	std::vector<char32_t> text = Utf8Decode(raw);
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
	if (autoSize) UpdateAutoScale(); // Reloadボタンで内容が変わった場合もlocalScaleを追従させる

	// 再読み込みのたびにeditBuffer_をファイルの最新内容へ同期する。編集途中の未保存の変更が
	// あっても、Load()を明示的に呼び直した時点ではファイル側を正とみなして上書きしてよい
	CopyToEditBuffer(editBuffer_, raw);
	editBufferLoaded_ = true;
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

void TextRenderComponent::SetTextProvider(TextProvider provider, Renderer* renderer) {
	textProvider_ = std::move(provider);
	// 呼び出し元がrendererを渡した場合のみ、登録直後に1回SetText()する（CreateDynamicTextObject
	// 等の生成直後に使う想定。次のUpdateDynamicTextComponentsを待たずに正しい箱サイズにできる）
	if (renderer != nullptr && textProvider_) {
		SetText(renderer, textProvider_());
	}
}

bool TextRenderComponent::SetText(Renderer* renderer, const std::string& utf8Text) {
	if (textureHandle == kTextureNone) return false; // LoadDynamic()が未実行・失敗している

	std::vector<char32_t> text = Utf8Decode(utf8Text);
	TextBitmap bitmap;
	if (!builder_.BuildFixed(text, fontSize * superSample, lineSpacing, canvasWidth, canvasHeight, bitmap)) return false;

	bool updated = renderer->UpdateTextureFromPixels(textureHandle, bitmap.width, bitmap.height, bitmap.rgbaPixels.data());

	if (updated && autoSize) {
		uint32_t measuredWidth = 0, measuredHeight = 0;
		// BuildFixedとは別に、実際にテキストが占める範囲だけを測る（ラスタライズはしない軽量パス）。
		// 空文字列（測定失敗）の場合は箱を潰さず直前のbitmapWidth_/Height_・localScaleをそのまま残す
		if (builder_.MeasureText(text, fontSize * superSample, lineSpacing, measuredWidth, measuredHeight)) {
			bitmapWidth_ = measuredWidth;
			bitmapHeight_ = measuredHeight;
			UpdateAutoScale();
		}
	}

	return updated;
}

bool TextRenderComponent::UpdateDynamicText(Renderer* renderer) {
	if (!dynamicText || !textProvider_) return false;
	return SetText(renderer, textProvider_());
}

void TextRenderComponent::UpdateAutoScale() {
	// userScaleMultiplier（Inspectorで調整、JSON保存済み）を今の文字列の実寸(GetNativeWidth/Height)に
	// 掛けるだけのシンプルな計算にする。GameObject共有のTransform.scaleではなく自分専用のlocalScaleを
	// 書き換えるため、1GameObjectに複数のTextRenderComponentがあってもサイズ変更が互いに干渉しない
	localScale.x = GetNativeWidth()  * userScaleMultiplier;
	localScale.y = GetNativeHeight() * userScaleMultiplier;
}

void TextRenderComponent::ApplyEditedText() {
	if (dynamicText || txtFilePath.empty() || !renderer_) return;

	// editBuffer_の内容をtxtFilePathへ書き戻してからLoad()し直す。Load()自体もファイルを
	// 読み直してeditBuffer_を同期するため、ここではファイル書き込みだけ行えばよい
	std::ofstream txtFile(StringUtils::ConvertString(txtFilePath), std::ios::binary | std::ios::trunc);
	if (!txtFile.is_open()) {
		Logger::Log(std::format("TextRenderComponent::ApplyEditedText: failed to write txt file '{}'\n", txtFilePath));
		return;
	}
	txtFile << editBuffer_;
	txtFile.close();

	Load(renderer_);
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

	// 箱の大きさ(localScale)とフォントサイズ(ラスタライズ解像度)は独立させる。テクスチャは箱に
	// 合わせて伸縮する（fontSizeを上げると同じ箱の中で文字が高解像度になるだけで、箱自体は変わらない）。
	// is3D（SpriteRenderComponentと同じ意味）に応じてDrawSprite3D/2Dを切り替える

	// GameObject共有のtransformをそのまま使わず、localOffset（このTextだけの位置ずらし。
	// 1GameObjectに複数のTextRenderComponentがあるとき重ならないよう並べるために使う）と
	// localScale（このTextだけの箱サイズ）を合成した一時Transformで描画する。
	// GameObject本体のtransform.scaleは常に{1,1,1}のまま関与しない
	Transform drawTransform = transform;
	drawTransform.translation = transform.translation + localOffset;
	drawTransform.scale = localScale;

	// Sprite2D/3Dのクアッド頂点は中心原点(-0.5〜0.5)固定のため、drawTransform.translationは常に
	// 「テキストの中心」を指す。kLeft/kRightは上記の位置（GameObject+localOffset）は変えずに、
	// 描画用に一時的にtranslation.xだけを箱の半幅分オフセットすることで実現する
	// （kLeft: 左端になるよう右へ半幅シフト、kRight: 右端になるよう左へ半幅シフト）
	if (horizontalAlign == HorizontalAlign::kLeft) {
		drawTransform.translation.x += localScale.x * 0.5f;
	} else if (horizontalAlign == HorizontalAlign::kRight) {
		drawTransform.translation.x -= localScale.x * 0.5f;
	}

	if (is3D) {
		renderer->DrawSprite3D(drawTransform, color, textureHandle, lighting, UVTransform{}, blendMode, blendStrength, alphaTest, alphaThreshold);
	} else {
		renderer->DrawSprite2D(drawTransform, color, textureHandle, lighting, UVTransform{}, blendMode, blendStrength, alphaTest, alphaThreshold);
	}
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

	// GameObjectのtranslationからの相対オフセット。1GameObjectに複数のTextRenderComponentが
	// あるとき、ここをずらすことで重ならないよう並べられる（例：Y方向に行間分だけずらして
	// 縦に並べる）。GameObject自体のtranslation（Gizmoで動かす対象）はTextの数に関わらず1つのまま
	std::string offsetLabel = std::string(namePrefix) + "位置オフセット（このTextだけ）";
	ImGui::DragFloat3(offsetLabel.c_str(), &localOffset.x, 1.0f);

	// このTextだけの箱サイズ。autoSize有効時はSetText()のたびに上書きされるため編集不可にする
	// （編集したい場合は下の「拡縮倍率」を使う）。autoSizeが無い静的テキストや、autoSizeを
	// オフにした動的テキストではここが唯一の箱サイズ編集手段になる
	if (autoSize) ImGui::BeginDisabled();
	std::string scaleLabel = std::string(namePrefix) + "箱サイズ（このTextだけ）";
	ImGui::DragFloat3(scaleLabel.c_str(), &localScale.x, 1.0f, 0.01f, 10000.0f);
	if (autoSize) ImGui::EndDisabled();

	// GameObjectのtranslationを基準にした水平方向の揃え（左詰め/中央/右詰め）。
	// translation自体は変えず、Draw()内で描画用に一時的にオフセットするだけなので、
	// Gizmo上での「位置」は切り替えても変化しない（見た目だけがtranslationを基準に揃う）
	{
		static const char* kHorizontalAlignLabels[] = { "左詰め", "中央", "右詰め" };
		int alignIndex = static_cast<int>(horizontalAlign);
		std::string alignLabel = std::string(namePrefix) + "水平揃え";
		if (ImGui::Combo(alignLabel.c_str(), &alignIndex, kHorizontalAlignLabels, 3)) {
			horizontalAlign = static_cast<HorizontalAlign>(alignIndex);
		}
	}

	std::string source = dynamicText ? std::string("（動的、SetText()で更新）") : ("テキスト: " + txtFilePath);
	std::string info = std::string(namePrefix) + " " + source + " / フォント: " + fontFilePath +
		" （焼き込み " + std::to_string(bitmapWidth_) + "x" + std::to_string(bitmapHeight_) + "px, 実寸 " +
		std::to_string(static_cast<int>(GetNativeWidth())) + "x" + std::to_string(static_cast<int>(GetNativeHeight())) + "px）";
	ImGui::Text("%s", info.c_str());

	// 自動サイズ調整の切り替え・拡縮倍率。dynamicTextはSetText()のたび、静的テキストはLoad()の
	// たび（＝テキスト再読み込み時）に適用される。オフにすると箱サイズは下の「箱サイズ」欄の
	// 手動編集のみで変わる。オンの間はlocalScale.x/yが常にGetNativeWidth/Height()×
	// userScaleMultiplierへ上書きされるため、拡縮は「箱サイズ」欄ではなくここのスライダーで行う
	{
		std::string autoSizeLabel = std::string(namePrefix) + "自動サイズ調整";
		std::string scaleMultiplierLabel = std::string(namePrefix) + "拡縮倍率（アスペクト比固定）";
		ImGui::Checkbox(autoSizeLabel.c_str(), &autoSize);
		if (!autoSize) ImGui::BeginDisabled();
		ImGui::DragFloat(scaleMultiplierLabel.c_str(), &userScaleMultiplier, 0.05f, 0.1f, 20.0f);
		if (!autoSize) ImGui::EndDisabled();
	}

	// dynamicTextは毎フレームSetText()が呼ばれる想定のため、Reloadボタン（静的txt読み込み用）は出さない
	if (!dynamicText) {
		std::string reloadLabel = std::string(namePrefix) + "テキストを再読み込み";
		if (renderer_ && ImGui::Button(reloadLabel.c_str())) {
			Load(renderer_);
		}

		// txtFilePathの中身をInspector内で直接編集できるテキストボックス。「適用」を押すまでは
		// ファイル・ビットマップどちらにも反映しない（誤操作で上書きしないよう明示操作にする、
		// Reloadボタンと同じ方針）。編集開始前にeditBuffer_が空ならファイルから一度だけ読み込む
		if (!editBufferLoaded_ && !txtFilePath.empty()) {
			std::string raw;
			if (ReadTxtFileRaw(txtFilePath, raw)) CopyToEditBuffer(editBuffer_, raw);
			editBufferLoaded_ = true;
		}

		std::string editLabel = std::string(namePrefix) + "テキスト編集";
		std::string applyLabel = std::string(namePrefix) + "編集内容を適用";
		ImGui::InputTextMultiline(editLabel.c_str(), editBuffer_, sizeof(editBuffer_), ImVec2(0.0f, 120.0f));
		if (renderer_ && !txtFilePath.empty() && ImGui::Button(applyLabel.c_str())) {
			ApplyEditedText();
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
	out["is3D"] = is3D;
	out["autoSize"] = autoSize;
	out["userScaleMultiplier"] = userScaleMultiplier;
	out["horizontalAlign"] = static_cast<int>(horizontalAlign);
	out["localOffset"] = Vector3ToJson(localOffset);
	out["localScale"] = Vector3ToJson(localScale);
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
	is3D = in.value("is3D", is3D);
	autoSize = in.value("autoSize", autoSize);
	userScaleMultiplier = in.value("userScaleMultiplier", userScaleMultiplier);
	horizontalAlign = static_cast<HorizontalAlign>(in.value("horizontalAlign", static_cast<int>(horizontalAlign)));
	if (in.contains("localOffset")) localOffset = Vector3FromJson(in["localOffset"]);
	if (in.contains("localScale")) localScale = Vector3FromJson(in["localScale"]);
}
