#pragma once
#include "../../IComponent.h"
#include "../../../../Math/Easing.h"
#include <string>
#include <functional>

// 文字列（A〜Z（大文字小文字問わず）・0〜9、半角スペースは空白として1文字分だけ間隔を空ける）を
// Resources/Alphabet/{文字}.objの3Dモデルを1文字ずつ横に並べて表示するHUD向けコンポーネント。
// TextRenderComponent（フォントビットマップ焼き込み方式）とは別の見た目（3Dモデルの文字）が
// 欲しい場合に使う。
//
// このコンポーネント自身はGameObjectを生成する権限を持たない（IComponentはシーンを知らない）
// ため、実際に文字ごとの子GameObject（ModelRenderComponent付き）を生成・破棄するのは
// SceneBase::UpdateAlphabetTextComponents（毎フレーム、Render()から呼ばれる）が担当する。
// このコンポーネントはtext/charSpacing/charScale等の「設定」と、SceneBase側が前回何を
// 生成したかを検知するための内部状態だけを持つ
class AlphabetTextComponent : public IComponent {
public:
	// 表示したい文字列。A〜Z（大文字小文字問わず）・0〜9・スペースのみ対応。それ以外の文字は
	// 対応する.objが無いため無視され、その文字分の間隔だけ詰めずに残る。
	// textProviderが設定されている場合、SceneBase::UpdateAlphabetTextComponentsが毎フレーム
	// textProvider()の戻り値でここを上書きする（撃破数等、動的な値を表示する場合はtextを
	// 直接編集せずSetTextProvider()を使う）
	std::string text;

	// 「毎フレーム呼ばれ、表示したい文字列を返す」コールバック。TextRenderComponent::TextProviderと
	// 同じ役割（PlayScene::killCount_等、呼び出し元ごとに異なるデータをこのAlphabetTextに
	// 紐付けたい場合に使う）。未設定（空）の場合はtextをInspectorで直接編集する静的な運用になる
	using TextProvider = std::function<std::string()>;
	void SetTextProvider(TextProvider provider) { textProvider_ = std::move(provider); }

	// textProviderが設定されていれば、それを呼んでtextへ反映する。SceneBase::
	// UpdateAlphabetTextComponentsが毎フレーム呼ぶ（TextRenderComponent::UpdateDynamicTextと
	// 同じ位置付け）。未設定なら何もしない
	void UpdateTextFromProvider() {
		if (textProvider_) text = textProvider_();
	}

	// 1文字分の基本サイズ(GameObjectのlocalScale相当)。A.obj等は「-1〜1」程度の単位サイズで
	// エクスポートされている前提のため、charScale=1.0で概ね等身大の箱に収まる
	float charScale = 1.0f;

	// 文字の中心から次の文字の中心までのX方向の距離。charScaleに対して十分な余白を持たせないと
	// 文字同士が重なる（.objの実際の横幅次第で調整する）
	float charSpacing = 1.2f;

	// 半角スペース1文字分の幅（前後の文字の中心間距離）。既定値はcharSpacingと同じ（従来通り
	// 通常文字と同じ間隔で空ける挙動）だが、独立したパラメータにすることでスペースだけ広く/狭く
	// 調整できる。例えば単語間をより大きく空けたい場合にcharSpacingより大きくする、といった調整に使う
	float spaceWidth = 1.2f;

	// GameObjectのtranslationを基準にした水平方向の揃え。既定はkCenter（従来通り、文字列全体の
	// 中心がtranslationに来る）。kLeftにすると、文字数が増減してもtranslationが常に左端に
	// 固定される（例：名前入力欄で1文字打つたびに既存の文字が左右にずれて見えないようにする）
	enum class HorizontalAlign { kLeft, kCenter, kRight };
	HorizontalAlign horizontalAlign = HorizontalAlign::kCenter;

	// PlayButtonComponentのホバー演出等、「毎フレーム変わりうる」見た目のオーバーレイ値。
	// charScale/charSpacingと違い、これらを変更してもRebuildAlphabetTextChildren（子GameObjectの
	// 全削除・再生成）は起きない。SceneBase::UpdateAlphabetTextComponentsが毎フレーム、
	// 親GameObject（owner）のTransform.scaleにdisplayScaleMultiplierを、各文字の子GameObjectが持つ
	// ModelRenderComponent::colorにdisplayColorをそのまま書き込むだけの軽量な処理を行う
	float displayScaleMultiplier = 1.0f;
	Vector4 displayColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// trueにすると、SceneBase::RebuildAlphabetTextChildrenが子GameObject（1文字ずつ）を組み立てる際、
	// 各文字にSpawnMoveComponentを付けて「Z方向の奥から手前へイージングで登場する」演出を与える。
	// entranceCharDelayぶんずつ開始タイミングをずらすことで、文字が順番に（波及び順で）現れて見える
	bool useCharEntranceAnimation = false;

	// 1文字ごとの登場開始タイミングのずれ(秒)。0番目の文字は0秒後、1番目はentranceCharDelay秒後…
	// という具合に、文字列の左から右へ順に開始が遅れていく
	float entranceCharDelay = 0.05f;

	// 各文字のSpawnMoveComponentへそのまま渡すパラメータ（PlayScene::BuildEnemyFromTemplateDataの
	// hasSpawnMove分岐と同じ意味）。zOffsetはローカルZ（親からの相対、そのまま奥方向のオフセット）
	float entranceZOffset = 6.0f;
	float entranceDuration = 0.35f;
	Easing::Type entranceEasing = Easing::Type::kOutCubic;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// SceneBase::UpdateAlphabetTextComponentsが「前回子GameObjectを組み立てた時点の値」を
	// 控えておくために使う。text/charScale/charSpacing/spaceWidthのいずれかが今の値と食い違って
	// いたら子GameObjectを作り直す（文字列だけでなく、間隔・サイズをInspectorで調整した場合も
	// 即座に反映されるようにするため）
	std::string lastBuiltText;
	float lastBuiltCharScale = -1.0f;   // 初回は必ず不一致になるよう、charScaleが取り得ない値で初期化する
	float lastBuiltCharSpacing = -1.0f; // 同上
	float lastBuiltSpaceWidth = -1.0f;  // 同上
	// 初回は必ず不一致になるよう、horizontalAlignが取り得ない値（kCount相当の番兵）で初期化する
	HorizontalAlign lastBuiltHorizontalAlign = static_cast<HorizontalAlign>(-1);

private:
	TextProvider textProvider_; // 未設定時は空（std::functionのbool変換でチェックする）
};
