#pragma once
#include "RenderComponentBase.h"
#include "../../../Graphics/Text/TextBitmapBuilder.h"
#include "../../GameObject.h"
#include <functional>
#include <string>

// .txt（UTF-8）の中身を指定フォントで1枚の合成ビットマップに焼き込み、Sprite2Dと同じ
// スクリーン空間クアッド1枚として描画するコンポーネント。SpriteRenderComponentと違い、
// テクスチャはファイルからではなく実行時にフォント+文字列から動的生成する。
// txtFilePath/fontFilePathはModelRenderComponentのdirectoryPath/filenameと同じ理由（実行時
// ハンドルではなく再現可能なソース）でJSONに保存し、復元時はComponentRegistryのcreatorが
// Load()を呼び直す
class TextRenderComponent : public RenderComponentBase {
public:
	TextRenderComponent();

	// GameObjectに「.txtを読んで表示するText」として必要な一式（TransformComponentの2D設定＋
	// TextRenderComponent追加＋Load()＋箱の初期scale）をまとめてセットアップする。
	// PlayScene側は「GameObjectを作ってこれを呼ぶ」だけでよく、is2D等の設定を毎回書かずに済む
	static TextRenderComponent* CreateStatic(GameObject& obj, Renderer* renderer,
		const std::string& txtPath, const std::string& fontPath, float fontSize);

	// GameObjectに「SetText()で毎フレーム内容を差し替えるHUD用Text」として必要な一式を
	// まとめてセットアップする（Camera座標表示等）
	static TextRenderComponent* CreateDynamic(GameObject& obj, Renderer* renderer,
		const std::string& fontPath, float fontSize, uint32_t canvasWidth, uint32_t canvasHeight);

	// txtFilePath/fontFilePathを読み込み、合成ビットマップを作ってtextureHandleを更新する。
	// フォント/txtが見つからない場合はfalseを返し、textureHandleはkTextureNoneのまま（描画スキップ）
	bool Load(Renderer* renderer);

	// dynamicText用：txtFilePathではなくSetText()で毎フレーム内容を差し替える運用にする。
	// フォントだけ読み込み、(canvasWidth, canvasHeight)固定サイズのテクスチャを1回だけ確保する
	bool LoadDynamic(Renderer* renderer);

	// dynamicText用：LoadDynamic()後、毎フレーム呼んでよい。同じテクスチャハンドル・同じ
	// キャンバスサイズのまま中身だけをTextureManager::UpdatePixelsで書き換える
	// （CreateFromPixelsのように毎回新しいハンドルを発行しないため、ハンドル枯渇しない）
	bool SetText(Renderer* renderer, const std::string& utf8Text);

	// 「毎フレーム呼ばれ、表示したい文字列を返す」コールバック。Camera座標・HP・スコア等、
	// 呼び出し元ごとに異なるデータをこのTextに紐付けたい場合に使う（PlayScene等は個別に
	// snprintf+SetText()を書かず、生成時に1回SetTextProviderするだけでよくなる）
	using TextProvider = std::function<std::string()>;
	void SetTextProvider(TextProvider provider) { textProvider_ = std::move(provider); }

	// dynamicTextかつtextProviderが設定されていれば、それを呼んでSetText()する。
	// 未設定（providerを使わずSetText()を直接呼ぶ運用）なら何もせずfalseを返す。
	// 呼び出し元は「シーン内のTextRenderComponentを毎フレーム1回ずつこれで回す」だけでよい
	bool UpdateDynamicText(Renderer* renderer);

	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// Load()直後のビットマップ実寸(px)。superSample倍だけfontSizeより大きく焼いてある
	// （情報表示・デバッグ用途。箱の初期scaleにはGetNativeWidth/Heightの方を使う）
	uint32_t GetBitmapWidth() const { return bitmapWidth_; }
	uint32_t GetBitmapHeight() const { return bitmapHeight_; }

	// 表示上の基準サイズ(px) = ビットマップ実寸 / superSample。GameObjectの初期scaleを
	// これに合わせておくと、箱をsuperSample倍まで拡大してもビットマップの実解像度を
	// 超えないためぼやけない（箱の大きさ＝Transform.scaleとフォントサイズ＝ラスタライズ
	// 解像度は独立しているため、Draw()はtransform.scaleをそのまま使う。SpriteRenderComponentと同じ扱い）
	float GetNativeWidth() const { return static_cast<float>(bitmapWidth_) / superSample; }
	float GetNativeHeight() const { return static_cast<float>(bitmapHeight_) / superSample; }

	std::string txtFilePath;
	std::string fontFilePath;
	float fontSize = 32.0f;
	float lineSpacing = 1.2f;

	// 実際のラスタライズは fontSize * superSample で焼いておき、余裕を持たせる倍率。
	// 1.0だと箱を等倍以上に拡大した瞬間からぼやけ始める。2.0なら箱をsuperSample=2倍まで
	// 拡大してもビットマップの実解像度の範囲内に収まるためシャープなまま
	float superSample = 2.0f;

	// true: txtFilePathを使わずSetText()で内容を差し替える運用（Camera座標表示等のHUD向け）
	bool dynamicText = false;
	// dynamicText時に確保する固定テクスチャサイズ(px)。SetText()の文字列がこれを超えるとクリップされる
	uint32_t canvasWidth = 512;
	uint32_t canvasHeight = 48;

	// dynamicText時、SceneBase::hudDefinitions_のどのエントリのTextProviderを使うかを示すキー。
	// TextProvider自体はラムダのためJSONに保存できず、Load直後は空になるため、SceneBase::
	// RebindDynamicTextProvidersがこのキーでhudDefinitions_を引いて付け直す（GameObject::nameは
	// 自由に変更できてしまいキーとして不安定なため、専用フィールドとして分離してある）
	std::string hudKey;

private:
	TextBitmapBuilder builder_; // dynamicTextはLoad()のたびにフォントファイルを読み直さないよう永続化する
	uint32_t bitmapWidth_ = 0;
	uint32_t bitmapHeight_ = 0;
	Renderer* renderer_ = nullptr; // DrawImGuiの再読込ボタンからLoad()を呼び直すために保持
	TextProvider textProvider_; // 未設定時は空（std::functionのbool変換でチェックする）

	// TODO(debug): テキスト非表示バグ調査用の一時カウンタ。原因特定後に削除する
	mutable int debugDrawCounter_ = 0;
};
