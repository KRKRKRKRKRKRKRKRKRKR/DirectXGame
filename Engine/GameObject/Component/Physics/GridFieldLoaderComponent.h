// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include <string>
#include <vector>

// パイプ接続パズル企画の「完全手動配置」用コンポーネント。ランダム生成（GridItemSpawnComponent/
// GridWallSpawnComponent）とは別に、Resources/GridPuzzle/Field/配下に置いたテキストファイルを
// 読み込んで、その通りに盤面のアイテム・壁を並べ直す機能を持つ。確認・レベルデザイン用
// （意図した配置を毎回同じ形で再現したい場合に使う。ランダム生成を置き換えるものではなく、
// Inspectorから明示的に「読み込む」ボタンを押した時だけ発動する追加の手段）。
//
// ファイル形式（GridPuzzleScene::LoadFieldDataが解釈する）：
//   任意で先頭に「R=3」「G=3」「B=3」のような取得目標数指定を0〜3行（順不同・省略可）、
//   続けて7x7の文字グリッド（各文字はR=赤アイテム/G=緑アイテム/B=青アイテム/W=壁/0=空白マス）。
//   取得目標数の指定を省略した色は、読み込み時にその色の敵HPバーの現在の設定を変更しない
//
// このコンポーネント自身はGameObjectを生成する権限を持たない（IComponentはシーンを知らない）
// ため、実際のファイル読み込み・パース・アイテム/壁GameObjectの生成はGridPuzzleScene::
// ApplyManualFieldIfRequestedが担当する。このコンポーネントはどのファイルを選んでいるかという
// 設定と、「読み込む」ボタンが押された一時的な要求フラグだけを持つ（GridItemSpawnComponent::
// ConsumeResetRequestedと同じ方針）。
class GridFieldLoaderComponent : public IComponent {
public:
	// Resources/GridPuzzle/Field/配下にある.txtファイルの一覧を、拡張子を除いた名前で返す
	// （フォルダが無い・空の場合は空を返す）。DrawImGuiがドロップダウンを描画するたびに呼ぶ
	// （ファイル数は少数想定のため、毎回スキャンし直しても軽量）
	static std::vector<std::string> ScanAvailableFieldNames();

	// 現在ドロップダウンで選択中のファイル名（拡張子を除く）。空文字なら未選択
	std::string selectedFieldName;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// DrawImGuiの「読み込む」ボタンが押された瞬間だけtrueを返し、呼ぶとフラグを消費する
	// （GridItemSpawnComponent::ConsumeResetRequestedと同じワンショットの取り出し方）。
	// GridPuzzleScene::ApplyManualFieldIfRequestedが毎フレームこれを見て、trueなら
	// selectedFieldNameのファイルを読み込んで盤面を作り直す。実行時の一時要求のため
	// ToJson/FromJsonでは保存しない
	bool ConsumeLoadRequested() {
		bool result = loadRequested_;
		loadRequested_ = false;
		return result;
	}

private:
	bool loadRequested_ = false;
};
