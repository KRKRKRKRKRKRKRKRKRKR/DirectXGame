#pragma once
#include "SceneBase.h"
#include <random>
#include <vector>

// パイプ接続パズル企画の最初のプロトタイプ画面。今回は企画書のうち「プレイヤー移動」「アイテム」
// だけを切り出して確認するための実装で、壁・HP・ダメージ計算・Undo・盤面リセットは含まない。
// 盤面（GridBoardComponent、列数・行数・マス間隔はInspectorで調整可能）をタイル
// （CubeRenderComponent）で表示し、中央にプレイヤー（GridBoardPlayerComponent。ReflexPlayerComponent
// には依存しない独立実装。同じ行/列上のマスをクリックして経路予約→実行フェーズで移動、
// 1マスごとにコストを消費するコスト制）を1体、固定座標にアイテム（GridItemComponent、
// 攻撃力+1／コスト+2固定／コスト±4リスキーの3種）を配置する画面。アイテムの取得判定は
// マス座標比較ではなく、Player/Item双方に付与したOBBColliderComponent(isTrigger=true)による
// ColliderSystemの当たり判定（重なった瞬間にGridItemComponent::OnTriggerEnterが発火）で行う。
// 盤面サイズ・マス間隔はGridBoardComponentが唯一のデータソース。
// SceneBase（GameObjectエディタ機能一式）をそのまま使い、PlayScene（REFLEX固有の計画/実行
// フェーズ等）は経由しない
class GridPuzzleScene : public SceneBase {
protected:
	void OnInitialize() override;
	void HandleSceneTransitionInput() override;

private:
	// OnInitialize()はLoadScene()より前に呼ばれる（SceneBase::Initialize参照）ため、その時点では
	// まだResources/GridPuzzle/scene.jsonの内容が読み込まれていない。trueで初期化し、
	// HandleSceneTransitionInputの最初の呼び出しで一度だけEnsureInitialObjectsExist()を実行して
	// falseに落とす（TutorialScene::needsInitialSpawn_と同じパターン。LoadScene()完了後に
	// 必ず呼ばれるこのタイミングまで、盤面・プレイヤーが既に読み込まれているかどうか判定できない
	// ため待つ）
	bool needsInitialSpawn_ = true;

	// 盤面フォルダ（GridBoardComponent付き）・光源・プレイヤーのうち、まだ存在しないものだけを
	// 新規に組み立てる（scene.jsonから読み込み済みなら何もしない）。タイル自体はここでは作らない
	// （excludeFromSave=trueで保存対象外のため、常にRebuildTilesIfBoardSizeChangedが作る）。
	// 以前はここで毎回問答無用に全部消して作り直していたが、それだと保存したプレイヤーの
	// 見た目設定・GridBoardComponentの色設定等が次回起動時に必ず初期値へ戻ってしまうバグに
	// なっていたため、「無ければ作る」方式に変更した
	void EnsureInitialObjectsExist();

	// 盤面フォルダ配下のタイル（tag==kGridCellTag、いずれもexcludeFromSave=true）だけを全部消して、
	// columns×rows個で作り直す。プレイヤーには一切触れない
	void RebuildTiles(int columns, int rows);

	// row*columns+col の順でタイルGameObjectへの非所有ポインタを保持する（RebuildTilesが
	// 作り直すたびに詰め直す）。UpdateTileHighlightsが色を書き換える対象を探すために使う
	std::vector<GameObject*> tileObjects_;

	// 直近にRebuildTilesした時点の列数・行数・マス間隔。HandleSceneTransitionInputが毎フレーム
	// GridBoardComponent::columns/rows/cellSpacingと比較し、値が変わっていたらRebuildTilesを
	// 呼び直す（＝Inspectorで列数・行数・マス間隔を変えるだけで盤面の見た目がその場で変わる）
	int lastBoardColumns_ = 0;
	int lastBoardRows_ = 0;
	float lastBoardCellSpacing_ = 0.0f;

	// 毎フレーム呼ぶ。GridBoardComponent::columns/rows/cellSpacingが直前の構築時から
	// 変わっていればタイルを作り直す
	void RebuildTilesIfBoardSizeChanged();

	// 毎フレーム呼ぶ。プレイヤーのGridBoardPlayerComponentが計画フェーズ中に返す
	// 「次にクリックできるマス」一覧（GetValidTargets、残コストで届く縦横のマス）を取得し、
	// 該当するタイルだけハイライト色にする（それ以外は市松模様の基本色に戻す）
	void UpdateTileHighlights();

	// 毎フレーム呼ぶ。GridBoardPlayerComponentは実行フェーズ完了時に自分自身で計画フェーズへ
	// 自動遷移するため（ReflexPlayerComponentのような、シーン側がFinishPreparing()を呼んで
	// 明示的に戻す準備フェーズを持たない）、現在は何もしない
	void AdvanceTurnIfExecutionFinished();

	// 毎フレーム呼ぶ。SceneBase::Render内のcolliderSystem_.ResolveAndDraw（このメソッドより前に
	// 実行される）がPlayer/Itemの重なりを検知してGridItemComponent::OnTriggerEnterを発火させ、
	// 効果適用済みのアイテムはtriggered=trueになっている。ここではtriggered==trueの
	// tag==kGridItemTagを検索し、削除せずに空きマス（他のアイテム・プレイヤーの現在マスと
	// 重ならないマス）をランダムに選んでcol/rowを書き換え、triggeredをfalseへ戻す
	// （＝1体拾うたびに即座に別の場所へリスポーンし、常に盤面上に3体存在し続ける）。
	// 実際の座標反映（Transform.translation）はSyncItemsに任せる
	void RespawnTriggeredItems();

	// 毎フレーム呼ぶ。シーン内にtag==kGridItemTagが1つも存在しなければ（起動直後）、
	// 固定座標(kInitialItemPlacements)へ攻撃力+1／コスト+2固定／コスト±4リスキーの3種を配置する
	// （初回配置専用。2回目以降はRespawnTriggeredItemsが1体ずつ個別に場所を変えるため出番がない）
	void RespawnItemsIfNoneExist();

	// RespawnTriggeredItems/RespawnItemsIfNoneExistの空きマス抽選に使う乱数生成器
	std::mt19937 rng_{ std::random_device{}() };

	// 毎フレーム呼ぶ。tag==kGridItemTagの各GameObjectについて、GridItemComponent::color
	// （Inspectorで調整可能）を兄弟のCubeRenderComponent::colorへ、col/row（配置マス座標）を
	// GridBoardComponent::GridToWorld経由でTransform.translationへ同期する。アイテムの色・
	// 位置はGridItemComponent側（col/row・color）が真の値で、実際の見た目（CubeRenderComponent::
	// color・Transform.translation）は常にそれに追従させる。Inspectorで手動でGridItemComponentを
	// Add Componentしてcol/rowを入力するだけで、対応するマスへ自動的に移動して表示される
	void SyncItems();
};
