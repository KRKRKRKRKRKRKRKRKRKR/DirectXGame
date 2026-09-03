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
	// text==""の場合は何も描画しない状態（textureHandle=kTextureNone）にする
	void Rebuild(Renderer* renderer);

private:
	Renderer* renderer_ = nullptr; // DrawImGuiの保存/削除ボタンからRebuild()を呼び直すために保持
	TextBitmapBuilder builder_;    // Rebuild()のたびにフォントファイルを読み直さないよう永続化する

	// ImGui::InputTextMultilineは素のchar[]バッファを要求するため、text（確定済み文字列）とは
	// 別に編集中の内容を持つ。「編集」ボタンでtextから読み込み直し、「保存」で書き戻す
	char editBuffer_[2048] = "";
	bool editBufferInitialized_ = false; // 初回DrawImGuiでtextの内容をeditBuffer_へ読み込み済みか
};
