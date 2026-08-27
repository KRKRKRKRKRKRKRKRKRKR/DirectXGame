#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// ランキング画面の行・列の表示だけを担当するコンポーネント。DashedLineComponent/
// AlphabetTextComponentと同じ設計：このコンポーネント自身は設定値と内部状態だけを持ち、
// 実際の行GameObject（1位〜N位を1行ずつ、さらに「順位」「名前」「スコア」の3列に分けた
// AlphabetTextComponent付きの孫）の生成・破棄はSceneBase側ではなくRankingScene::
// UpdateRankingComponent（Scene固有の処理のためSceneBaseへは汎用化していない）が行う。
//
// スクロール操作（マウスホイール・スクロールバードラッグ・自動フォーカス演出）は一切ここに
// 持たない。以前はこのコンポーネントがscrollOffset等のスクロール状態も持っており、
// 「表示の基準位置」「カメラの初期位置」「スクロール量」という複数のGameObjectをまたいだ値の
// 整合を取る必要があって不具合の温床になっていた。「カメラだけが上下に動く。この
// コンポーネントは行・列を表示するだけ」という設計に変更し、スクロール関連は全て
// RankingCameraScrollerComponent（カメラGameObject側）に移した
// （Engine/GameObject/Component/Camera/RankingCameraScrollerComponent.h参照）。
// 依存の向きは一方向：このコンポーネントが行を並べる → カメラ側コンポーネントがそれを
// 動かして見せる、であり、逆方向の参照は無い
class RankingComponent : public IComponent {
public:
	// 1行目(1位側)のY座標（ownerのtranslationからの相対オフセット）。2行目以降はここから
	// rowSpacingぶんずつ引いた位置に積み上げる（固定、再構築時のみ計算。以後動かさない）
	float rowStartY = 4.0f;

	// 行の中心から次の行の中心までのY方向の距離。RankingCameraScrollerComponent::rowSpacingと
	// 同じ値にすること（スクロール計算側が参照する値のため。依存を一方向にする代償として値が
	// 重複しているので、行間隔を変えたらカメラ側の設定も合わせて調整する必要がある）
	float rowSpacing = 0.8f;

	// 全行共通のZ座標（ownerのtranslationからの相対オフセット）
	float rowZ = 0.0f;

	// 各行(AlphabetTextComponent)のcharScale/charSpacingへそのまま渡す値
	float charScale = 1.0f;
	float charSpacing = 1.2f;

	// 「順位」「名前」「スコア」を1行にまとめた1個のAlphabetTextComponentではなく、列ごとに
	// 独立した3個のAlphabetTextComponent（子）として並べ、各列を個別に左揃えできるようにする。
	// 値はownerのtranslationからの相対Xオフセット（ownerの子のローカル座標）。列の横幅は
	// 文字数によって変わる（AlphabetTextComponent::HorizontalAlign::kLeftのため、Xオフセットを
	// 大きくするほど右へ列自体がずれる、という単純な位置指定になる）
	float rankColumnX = -4.0f;
	float nameColumnX = -1.5f;
	float scoreColumnX = 2.0f;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// RankingScene::UpdateRankingComponentが「前回、行の子GameObjectを組み立てた時点の
	// エントリ総数」を控えておくために使う。RankingManager::GetEntries().size()と食い違っていたら
	// （リセットや新規Submitで件数が変わったら）行の子GameObjectを作り直す
	size_t lastBuiltEntryCount = static_cast<size_t>(-1); // 初回は必ず不一致になるようあり得ない値にする
	float lastBuiltRowStartY = 0.0f;
	float lastBuiltRowSpacing = -1.0f;
	float lastBuiltRowZ = 0.0f;
	float lastBuiltCharScale = -1.0f;
	float lastBuiltCharSpacing = -1.0f;
	float lastBuiltRankColumnX = 0.0f;
	float lastBuiltNameColumnX = 0.0f;
	float lastBuiltScoreColumnX = 0.0f;

	// entryCount（RankingManager::GetEntries().size()、このコンポーネント自身はRankingManagerを
	// 知らないため呼び出し元から渡してもらう）を含めて、今の設定値がlastBuilt*と食い違っているか。
	// lastBuiltEntryCount/lastBuiltCharSpacing等が取り得ない値で初期化されているため、初回は必ずtrue
	bool NeedsRebuild(size_t entryCount) const {
		return entryCount != lastBuiltEntryCount ||
			rowStartY != lastBuiltRowStartY || rowSpacing != lastBuiltRowSpacing ||
			rowZ != lastBuiltRowZ || charScale != lastBuiltCharScale || charSpacing != lastBuiltCharSpacing ||
			rankColumnX != lastBuiltRankColumnX || nameColumnX != lastBuiltNameColumnX ||
			scoreColumnX != lastBuiltScoreColumnX;
	}

	void MarkBuilt(size_t entryCount) {
		lastBuiltEntryCount = entryCount;
		lastBuiltRowStartY = rowStartY;
		lastBuiltRowSpacing = rowSpacing;
		lastBuiltRowZ = rowZ;
		lastBuiltCharScale = charScale;
		lastBuiltCharSpacing = charSpacing;
		lastBuiltRankColumnX = rankColumnX;
		lastBuiltNameColumnX = nameColumnX;
		lastBuiltScoreColumnX = scoreColumnX;
	}

	// DrawImGuiの「ランキングをリセット」ボタンが押された瞬間にtrueが立つ。このコンポーネント
	// 自身はRankingManagerを知らない（Engine層はGame層に依存しない設計のため）ので、実際の
	// RankingManager::Reset()呼び出しはRankingScene::HandleSceneTransitionInputが毎フレーム
	// ConsumeResetRequested()で消費して行う（ComboPopupComponent::ConsumePendingRequestと同じ
	// 「リクエストを積むだけ、実処理はScene側」パターン）
	bool ConsumeResetRequested() {
		bool result = resetRequested_;
		resetRequested_ = false;
		return result;
	}

private:
	bool resetRequested_ = false;
};
