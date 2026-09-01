#pragma once
#include "SceneBase.h"
#include <vector>

// パイプ接続パズル企画の最初のプロトタイプ画面。今回は企画書のうち「プレイヤー移動」だけを
// 切り出して確認するための最小実装で、壁・アイテム・HP・ダメージ・Undo・敵は含まない。
// 盤面（GridBoardComponent、列数・行数はInspectorで調整可能）をタイル（CubeRenderComponent）で
// 表示し、中央にプレイヤー（GridReflexPlayerComponent。REFLEXのReflexPlayerComponentをそのまま
// 継承し、クリック位置の妥当性判定だけをグリッド制約でオーバーライドしたもの。クリック先は
// 直前の予約地点と同じ行/列上・1〜4マス先のタイルのみ有効、最大4回予約したら自動で実行フェーズへ
// 移行する）を1体置くだけの画面。盤面の列数・行数とプレイヤーの移動範囲
// （GridReflexPlayerComponent::gridWidth/gridHeight）は意図的に同期させない
// （互いに独立してInspectorから調整できるだけでよい、という設計判断）。
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
	// 作り直すたびに詰め直す）。UpdateTileHighlightsがGridClickJumpPlayerComponent::
	// GetValidTargets()の結果と突き合わせて、色を書き換える対象を探すために使う
	std::vector<GameObject*> tileObjects_;

	// 直近にRebuildTilesした時点の列数・行数。HandleSceneTransitionInputが毎フレーム
	// GridBoardComponent::columns/rowsと比較し、値が変わっていたらRebuildTilesを呼び直す
	// （＝Inspectorで列数・行数を変えるだけで盤面の大きさがその場で変わる）
	int lastBoardColumns_ = 0;
	int lastBoardRows_ = 0;

	// 毎フレーム呼ぶ。GridBoardComponent::columns/rowsが直前の構築時から変わっていればタイルを
	// 作り直す
	void RebuildTilesIfBoardSizeChanged();

	// 毎フレーム呼ぶ。プレイヤーのGridReflexPlayerComponentが計画フェーズ中に返す
	// 「次にクリックできるマス」一覧を取得し、該当するタイルだけハイライト色にする
	// （それ以外は市松模様の基本色に戻す）
	void UpdateTileHighlights();

	// 毎フレーム呼ぶ。基底のReflexPlayerComponentは実行フェーズ完了後、本来はPlayScene側が
	// 敵の補充スポーンを終えてFinishPreparing()を呼ぶまでPhase::kPreparingで足止めする設計だが、
	// このシーンには敵がいないため、実行フェーズ完了を検知した瞬間に即座にFinishPreparing()を
	// 呼んで計画フェーズへ戻す（呼ばないと2ターン目以降クリックしても経路を予約できなくなる）
	void AdvanceTurnIfExecutionFinished();

	// 毎フレーム呼ぶ。カメラのX座標だけをプレイヤーの現在位置へ指数減衰でなめらかに追従させる
	// （CameraFollowComponentと同じ減衰式）。Y/Zは常に固定のまま動かさない（縦長×横に長い盤面で、
	// 上下には動かず横方向だけプレイヤーを追いかける見た目にするため）。以前はPlayerの子に
	// CameraComponent付きGameObjectを置いて追従させようとしたが、シーン内にCameraComponentが
	// 1つでもあるとSceneBase::ResolveGameCameraがそちらをGameビュー用カメラとして自動優先して
	// しまい、プレイヤーの縦移動・イージングの揺れをそのまま拾って見づらくなる問題があったため、
	// GameObject/CameraComponentは使わず、SceneBase::camera_（このシーン専用の固定カメラ）を
	// 直接動かす方式に変更した
	void UpdateCameraFollow();

	// カメラが実際に追従している現在のX座標（指数減衰の途中経過）。OnInitialize()で盤面初期表示の
	// 中心Xへリセットする
	float cameraFollowX_ = 0.0f;
};
