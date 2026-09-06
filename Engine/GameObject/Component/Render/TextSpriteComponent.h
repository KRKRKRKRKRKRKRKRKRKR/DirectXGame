// 10DaysJam
#pragma once
#include "RenderComponentBase.h"
#include "../../../Graphics/Text/TextBitmapBuilder.h"
#include <string>

class Renderer;

// 2Dスクリーン空間UIとして文字列を描画するコンポーネント（削除済みTextRenderComponentの
// 作り直し版）。前バージョンは「静的テキスト」「毎フレーム内容が変わるHUD」「SceneBase側の
// HUD名テーブル・hudKeyでの照合・保存復元の付け直し」を1つに詰め込んでいて使いにくかった
// （経緯は memory: project_textrendercomponent_removed 参照）ため、今回はHUD機能を持たず
// 「文字を打つ→保存ボタンで確定→確定した文字列をラスタライズして表示するだけ」の
// 静的テキスト専用にした。常にTransformComponent::is2D=trueのスクリーン空間（px座標、
// 左上原点）に配置され、3Dワールドのカメラ移動の影響を受けない＝常にカメラに映り続ける。
class TextSpriteComponent : public RenderComponentBase {
public:
	// GameObjectのtranslation（px、スクリーン空間の左上原点）を基準にした水平方向の揃え。
	// translation自体は変えず、Draw()内で描画用に一時的にオフセットするだけ
	enum class HorizontalAlign { kLeft, kCenter, kRight };

	TextSpriteComponent();

	// 現在確定済みの表示文字列（UTF-8）。Inspectorの「保存」を押すまでは編集中の内容は
	// ここへ反映されない（誤操作で毎フレーム再生成されるのを防ぐため、明示操作にしてある）
	std::string text;

	float fontSize = 32.0f;
	float lineSpacing = 1.2f;
	HorizontalAlign horizontalAlign = HorizontalAlign::kCenter;
	std::string fontFilePath = "Resources/Font/font.ttf";

	// このテキストスプライトの表示/非表示をトグルするキーボードの数字キー（0〜9）。
	// -1は「割り当てなし」（従来通り、常にtext/textureHandleがあれば表示され続ける）。
	// Inspectorのコンボボックスから選ぶ。実際のキー入力監視・トグルはSceneBase::
	// UpdateTextSpriteVisibilityToggles（毎フレーム、Render()から呼ばれる）が行う
	int assignedNumberKey = -1;

	// 現在表示中かどうか。assignedNumberKeyが割り当てられている間は、対応する数字キーを
	// 押すたびにSceneBase::UpdateTextSpriteVisibilityTogglesがこれを反転させる。
	// assignedNumberKey==-1（割り当てなし）の場合はこの値を無視して常に表示する
	// （Draw参照。既存のtext入力だけで使う既存シーンの見た目を変えないため）
	bool isVisible = true;

	// 自動スナップ：Rebuild()のたびに、実際にラスタライズした文字列の実寸(px)へ自動的に
	// 合わせる「箱」のサイズ。GameObject共有のTransform.scaleではなくこちらを使うのは、
	// Gizmoで誤ってscaleを触っても次のRebuild()で必ず実寸へ戻るようにするため
	// （保存対象外：Rebuild()のたびに再計算する値のため、ToJson/FromJsonには含めない）
	float boxWidth = 0.0f;
	float boxHeight = 0.0f;

	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// text/fontSize/lineSpacing/fontFilePathから合成ビットマップを作り直し、textureHandle・
	// boxWidth/boxHeightを更新する（＝自動スナップ）。GameObject生成直後
	// （ComponentRegistration.cppのcreator）と、Inspectorの「保存」「削除」ボタンから呼ぶ。
	// text==""の場合は何も描画しない状態（textureHandle=kTextureNone）にする。
	// 直前のビットマップと同サイズ（幅・高さとも一致）ならRenderer::UpdateTextureFromPixelsで
	// 既存のtextureHandleの中身だけを差し替える（新規ハンドルを消費しない）。サイズが変わった
	// 場合、または初回（textureHandle==kTextureNone）はCreateTextureFromPixelsで新規作成する。
	// これは元々「保存ボタンで確定した時だけ呼ぶ」静的テキスト専用の想定だったが、
	// GridPuzzleScene::UpdateCostTextのように値が変わるたびに毎フレーム相当で呼ばれる
	// 準動的な使い方をすると、差し替え無しでは毎回新規ハンドルを消費し続けて
	// TextureManager::kMaxTextureCount（500）にすぐ到達してしまうため、Rebuild自体を
	// 使い回し可能な実装にしてある
	void Rebuild(Renderer* renderer);

private:
	Renderer* renderer_ = nullptr; // DrawImGuiの保存/削除ボタンからRebuild()を呼び直すために保持
	TextBitmapBuilder builder_;    // Rebuild()のたびにフォントファイルを読み直さないよう永続化する

	// 直前にRebuild()で作成/更新したビットマップの実寸(px)。次回Rebuild()時、同サイズなら
	// UpdateTextureFromPixelsで使い回せるかどうかの判定に使う（textureHandleが既に
	// kTextureNoneでなく、かつこのサイズと一致する場合のみ使い回す）
	uint32_t lastBitmapWidth_ = 0;
	uint32_t lastBitmapHeight_ = 0;

	// ImGui::InputTextMultilineは素のchar[]バッファを要求するため、text（確定済み文字列）とは
	// 別に編集中の内容を持つ。「編集」ボタンでtextから読み込み直し、「保存」で書き戻す
	char editBuffer_[2048] = "";
	bool editBufferInitialized_ = false; // 初回DrawImGuiでtextの内容をeditBuffer_へ読み込み済みか
};
