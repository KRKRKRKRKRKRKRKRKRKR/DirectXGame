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
	// GameObjectのtranslation（+localOffset）を基準に、テキストの箱（localScaleサイズの矩形）が
	// どちら向きに広がるかを決める。Sprite2D/3Dのクアッド頂点は中心原点(-0.5〜0.5)固定のため、
	// kLeft/kRightはDraw()内で描画用に一時的にtranslation.xをオフセットして実現する
	// （GameObject本体の位置は変更しない）
	enum class HorizontalAlign { kLeft, kCenter, kRight };

	TextRenderComponent();

	// GameObjectに「.txtを読んで表示するText」として必要な一式（TransformComponentの2D設定＋
	// TextRenderComponent追加＋Load()＋箱の初期scale）をまとめてセットアップする。
	// PlayScene側は「GameObjectを作ってこれを呼ぶ」だけでよく、is2D等の設定を毎回書かずに済む。
	// is3D=trueの場合はスクリーン空間(px座標)ではなく、通常の3Dワールド空間のTransformとして
	// セットアップする（SpriteRenderComponentのis3Dと同じ意味）
	static TextRenderComponent* CreateStatic(GameObject& obj, Renderer* renderer,
		const std::string& txtPath, const std::string& fontPath, float fontSize, bool is3D = false);

	// GameObjectに「SetText()で毎フレーム内容を差し替えるHUD用Text」として必要な一式を
	// まとめてセットアップする（Camera座標表示等）
	static TextRenderComponent* CreateDynamic(GameObject& obj, Renderer* renderer,
		const std::string& fontPath, float fontSize, uint32_t canvasWidth, uint32_t canvasHeight, bool is3D = false);

	// txtFilePath/fontFilePathを読み込み、合成ビットマップを作ってtextureHandleを更新する。
	// フォント/txtが見つからない場合はfalseを返し、textureHandleはkTextureNoneのまま（描画スキップ）
	bool Load(Renderer* renderer);

	// dynamicText用：txtFilePathではなくSetText()で毎フレーム内容を差し替える運用にする。
	// フォントだけ読み込み、(canvasWidth, canvasHeight)固定サイズのテクスチャを1回だけ確保する
	bool LoadDynamic(Renderer* renderer);

	// dynamicText用：LoadDynamic()後、毎フレーム呼んでよい。同じテクスチャハンドル・同じ
	// キャンバスサイズのまま中身だけをTextureManager::UpdatePixelsで書き換える
	// （CreateFromPixelsのように毎回新しいハンドルを発行しないため、ハンドル枯渇しない）。
	// autoSize=trueの場合、実際の文字列サイズをTextBitmapBuilder::MeasureTextで測り、
	// localScaleをそれに追従させる（詳しくはUpdateAutoScale参照）。GameObject共有のTransformには
	// 触れない（1GameObjectに複数のTextRenderComponentを付けたとき、互いのサイズ変更が
	// 干渉しないようにするため。詳しくはlocalScale/localOffsetのコメント参照）
	bool SetText(Renderer* renderer, const std::string& utf8Text);

	// 「毎フレーム呼ばれ、表示したい文字列を返す」コールバック。Camera座標・HP・スコア等、
	// 呼び出し元ごとに異なるデータをこのTextに紐付けたい場合に使う（PlayScene等は個別に
	// snprintf+SetText()を書かず、生成時に1回SetTextProviderするだけでよくなる）。
	// rendererを渡すと、登録直後に1回だけSetText()を実行してlocalScaleを確定させる
	// （CreateDynamic直後はcanvasWidth/canvasHeight丸ごとの大きな箱のままのため、次のUpdate
	// フレームを待たずに生成直後から正しい箱サイズで表示させたい場合に使う）
	using TextProvider = std::function<std::string()>;
	void SetTextProvider(TextProvider provider, Renderer* renderer = nullptr);

	// dynamicTextかつtextProviderが設定されていれば、それを呼んでSetText()する。
	// 未設定（providerを使わずSetText()を直接呼ぶ運用）なら何もせずfalseを返す。
	// 呼び出し元は「シーン内のTextRenderComponentを毎フレーム1回ずつこれで回す」だけでよい
	bool UpdateDynamicText(Renderer* renderer);

	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// Load()直後のビットマップ実寸(px)。superSample倍だけfontSizeより大きく焼いてある
	// （情報表示・デバッグ用途。箱の初期サイズにはGetNativeWidth/Heightの方を使う）
	uint32_t GetBitmapWidth() const { return bitmapWidth_; }
	uint32_t GetBitmapHeight() const { return bitmapHeight_; }

	// 表示上の基準サイズ(px) = ビットマップ実寸 / superSample。箱の初期サイズをこれに
	// 合わせておくと、箱をsuperSample倍まで拡大してもビットマップの実解像度を超えないため
	// ぼやけない（箱の大きさ＝localScaleとフォントサイズ＝ラスタライズ解像度は独立しているため、
	// Draw()はlocalScaleをそのまま使う。SpriteRenderComponentと同じ扱い）
	float GetNativeWidth() const { return static_cast<float>(bitmapWidth_) / superSample; }
	float GetNativeHeight() const { return static_cast<float>(bitmapHeight_) / superSample; }

	// GameObjectのtranslationからの相対オフセット。1GameObjectに複数のTextRenderComponentを
	// 付けたとき（例：撃破数とコンボを同じ位置基準で縦に並べる）、各Textをずらして重ならないよう
	// 配置するために使う。GameObject本体の位置（translation、Gizmoで動かす対象）はTextの数に
	// 関わらず1つのまま、各Textの見た目位置だけがtranslation + localOffsetになる
	Vector3 localOffset{ 0.0f, 0.0f, 0.0f };

	// このTextRenderComponent自身の箱サイズ。GameObject共有のTransform.scaleではなくこちらを
	// 使うのは、1GameObjectに複数のTextRenderComponentがある場合、autoSizeによる自動リサイズが
	// 互いに干渉しない（Aの文字数変化でBの箱サイズが勝手に変わらない）ようにするため。
	// CreateStatic/CreateDynamicが初期値をGetNativeWidth/Heightで設定し、以降はautoSize有効時
	// UpdateAutoScaleが更新する（GameObject.GetTransform().scaleは常に{1,1,1}のまま触らない）
	Vector3 localScale{ 1.0f, 1.0f, 1.0f };

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

	// SpriteRenderComponent::is3Dと同じ意味。falseならSprite2Dと同じスクリーン空間UI
	// （Renderer::DrawSprite2D、TransformComponent::is2D=trueのpx座標）、trueなら通常の
	// 3Dワールド空間オブジェクト（Renderer::DrawSprite3D、is2D=falseの通常Transform）として描画する
	bool is3D = false;
	// dynamicText時に確保する固定テクスチャサイズ(px)。SetText()の文字列がこれを超えるとクリップされる
	uint32_t canvasWidth = 512;
	uint32_t canvasHeight = 48;

	// GameObjectのtranslationを基準にした水平方向の揃え。既定は従来通りのkCenter
	// （translationがテキストの中心）。「Kills: 5」→「Kills: 12」のように文字数が変わっても
	// 左端/右端をtranslationに固定したい場合はkLeft/kRightを使う
	HorizontalAlign horizontalAlign = HorizontalAlign::kCenter;

	// dynamicText時、SceneBase::hudDefinitions_のどのエントリのTextProviderを使うかを示すキー。
	// TextProvider自体はラムダのためJSONに保存できず、Load直後は空になるため、SceneBase::
	// RebindDynamicTextProvidersがこのキーでhudDefinitions_を引いて付け直す（GameObject::nameは
	// 自由に変更できてしまいキーとして不安定なため、専用フィールドとして分離してある）
	std::string hudKey;

	// true: dynamicTextの内容が変わるたび（SetText()のたび）、実際の文字列サイズに合わせて
	// localScaleを自動更新する。「Kills: 5」→「Kills: 12」のように桁数が増減しても、常に
	// 文字列ぴったりの箱サイズになる（従来はcanvasWidth/canvasHeight固定サイズのままだったため、
	// 短い文字列だと余白だらけの透明な箱が広く残っていた）。
	// localScale.x/yには常にGetNativeWidth/Height()（今の文字列の実寸）× userScaleMultiplierが
	// 入るため、アスペクト比は常にテキストの自然な縦横比のまま拡縮される
	bool autoSize = true;

	// autoSize適用時、GetNativeWidth/Height()（今の文字列の実寸）に掛ける倍率。1.0が等倍
	// （実寸そのまま）。Inspectorのスライダーで拡大したい場合はここを変更する（Gizmoで直接
	// scaleをドラッグしても、次のSetText()でこの倍率に基づく値へ上書きされてしまうため、
	// 拡縮はここを編集して行う）。
	// 以前は「transform->scale ÷ 直前のNativeサイズ」で倍率を逆算していたが、シーンロード時に
	// LoadDynamic()がbitmapWidth_をcanvasサイズへリセットする一方、transform.scaleはJSONに
	// 保存された実測値ベースの値のまま残るため両者が食い違い、次のSetText()で不正な倍率へ
	// 化けてフォントが巨大化するバグがあった。専用メンバとして明示的に持ち、JSONにも
	// 保存することで「今の倍率」を常に正として扱う
	float userScaleMultiplier = 1.0f;

private:
	// autoSize/userScaleMultiplierの設定に従い、bitmapWidth_/bitmapHeight_確定後にlocalScaleを
	// 書き換える。SetText()内、実測サイズ取得に成功した場合のみ呼ばれる
	void UpdateAutoScale();

	// Inspector内のテキストボックスでtxtFilePathの中身を直接編集できるようにする。
	// editBuffer_はテキストボックスの表示専用（未保存の編集中身）で、「適用」を押すまでは
	// txtFilePath自体にもビットマップにも反映しない（誤操作でファイルを上書きしないため）
	void ApplyEditedText();

	// ImGui::InputTextMultilineは素のchar[]バッファを要求する（imgui_stdlib未導入のため、
	// SceneBase.cppのstaticTextContentBufと同じ固定サイズバッファ方式に合わせる）
	char editBuffer_[4096] = "";
	bool editBufferLoaded_ = false; // txtFilePathの中身をeditBuffer_へ読み込み済みか

	TextBitmapBuilder builder_; // dynamicTextはLoad()のたびにフォントファイルを読み直さないよう永続化する
	uint32_t bitmapWidth_ = 0;
	uint32_t bitmapHeight_ = 0;
	Renderer* renderer_ = nullptr; // DrawImGuiの再読込ボタンからLoad()を呼び直すために保持
	TextProvider textProvider_; // 未設定時は空（std::functionのbool変換でチェックする）
};
