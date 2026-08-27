#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/Easing.h"

// ランキング画面のスクロール操作を1つに集約するコンポーネント。カメラGameObject自身に付ける
// （RankingScene::FindRankingCameraが探すCameraComponent持ちのGameObjectと同じ相手）。
//
// 以前はスクロール状態（scrollOffset等）をRankingComponent（行・列の表示を担当する別の
// GameObject）が持っており、「表示の基準位置」「カメラの初期位置」「スクロール量」という
// 複数のGameObjectをまたいだ値の整合を取る必要があって不具合の温床になっていた
// （カメラの初期translationがscene.json編集で1位からズレて保存されると、スクロールを
// 一番上まで戻しても1位が表示されない、等）。
// 「カメラだけが上下に動く。RankingComponentは行・列を表示するだけ」というユーザー指定の
// 方針に合わせ、スクロール・スクロールバー・Titleボタン・自動フォーカス演出の設定と状態を
// すべてこのコンポーネント（カメラ側）にまとめ、依存の向きを一方向にする：
// RankingComponentが行を並べる → このコンポーネントがカメラのtranslation.yを動かす。
//
// 行の間隔（rowSpacing）だけはRankingComponent側の値をRankingSceneが読み取って使う
// （このコンポーネント自身は値を持たない）。以前はここにも同じ意味のrowSpacingフィールドを
// 重複して持たせていたが、2箇所を手で同期する必要があるせいで値がズレ、「スクロール可能範囲が
// 実際の行間隔と食い違い、途中までしかスクロールできない」不具合の直接原因になった。
// 値の置き場所を1つ（RankingComponent側）に統一することで、行間隔を変えてもスクロール側は
// 自動的に正しい値を使う（書き込みは行わない、読み取り専用の一方向参照）。
//
// スクロール可能範囲の自動クランプ（かつてvisibleRowCountをfov/カメラ距離から動的計算していた
// 仕組み）とスクロールバーは廃止した。過去に別プロジェクトで作ったランキング実装
// （Novice/2Dスプライトベース、W/Sキーでカメラを直接動かすだけ・上下限のクランプ無し）を
// 参照した結果、「自動計算・クランプで完璧な範囲を求める」より「素朴に動かすだけ、行き過ぎても
// 実害が無い」方がシンプルで壊れにくいという判断による。スクロールは単純に、ホイール入力の
// 分だけscrollOffsetを加減算するだけで、上限・下限は設けない
class RankingCameraScrollerComponent : public IComponent {
public:
	// マウスホイール1クリックあたりのスクロール移動量（Y座標そのもの）の基準値。
	// rowSpacingを基準にした「1クリックで約半行分動く」感覚になるよう既定0.4倍にしてある
	float wheelSensitivity = 0.4f;

	// ---- Titleへ戻るボタン（カメラのワールド位置＋このオフセットで毎フレーム配置する）----
	float titleButtonX = -6.0f;
	float titleButtonY = -4.0f;
	float titleButtonZ = 0.0f;

	// ---- 自動フォーカス演出（Clear画面から遷移時、1位から自分の順位までイージングでスクロール）----
	float autoFocusDuration = 1.2f;
	Easing::Type autoFocusEasing = Easing::Type::kOutCubic;

	// 自分がSubmitした行をハイライトする色（RGBA）
	Vector4 highlightColor = { 1.0f, 0.85f, 0.2f, 1.0f };

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// ---- 実行時専用の状態（保存しない）----

	// スクロール量（Yワールド座標のオフセット）。0が1位（先頭）、負方向が末尾方向。
	// RankingScene::UpdateCameraScrollerが毎フレームcameraObj.translation.y = baseY + scrollOffset
	// へ反映する
	float scrollOffset = 0.0f;

	// 1位（先頭）の行が実際に画面へ表示される基準Y座標。RankingScene::UpdateCameraScrollerが、
	// 1位の行GameObjectのワールドYを見つけた最初のフレームに一度だけ捕捉する
	// （カメラ自身の初期translationは基準として使わない。scene.json上でカメラ位置が
	// 1位からズレて保存されていても、常に1位の実座標を基準にするため）
	float baseY = 0.0f;
	bool baseYCaptured = false;

	bool autoFocusPlaying = false;
	bool autoFocusApplied = false;
	float autoFocusElapsed = 0.0f;
	float autoFocusStartOffset = 0.0f;
	float autoFocusTargetOffset = 0.0f;
	int highlightEntryIndex = -1;
};
