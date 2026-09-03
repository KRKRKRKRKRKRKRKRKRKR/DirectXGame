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
// 取得したアイテムは即座に盤面へ戻さず、GridItemSpawnComponent::collectedDisplayTopを基準に
// 盤面外へ取得順に積み重ねて表示し（UpdateCollectedItemsDisplay）、プレイヤーの行動（実行フェーズ）が
// 全て終わったタイミングでまとめて空きマスへ再配置する（FinalizeCollectedItemsOnTurnEnd）。
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

	// 毎フレーム呼ぶ。GridBoardPlayerComponent::GetCurrentCost()/maxCost（行動可能マス数＝残り
	// 移動コスト）を文字列化し、前回表示した内容と違う時だけtag==kGridCostTextTagの
	// TextSpriteComponent::textを書き換えてRebuildする（値が変わらない間は毎フレームの
	// テクスチャ再生成を避けるため。TextSpriteComponentは元々「保存ボタンで確定した時だけ
	// Rebuildする」静的テキスト専用の設計だが、変更検知を自前で行えばこのような準動的な表示にも使える）
	void UpdateCostText();

	// 毎フレーム呼ぶ。GridBoardPlayerComponentの現在フェーズを前フレームと比較し、
	// 「実行フェーズ（kExecuting）からちょうど計画フェーズ（kPlanning）へ戻った瞬間」を検知したら
	// FinalizeCollectedItemsOnTurnEndを呼ぶ（GridBoardPlayerComponentはReflexPlayerComponentと
	// 異なり、シーン側が明示的に戻す準備フェーズを持たないため、シーン側でできることは
	// このタイミングを検知して他の処理をトリガーすることだけ）
	void AdvanceTurnIfExecutionFinished();

	// 直前フレームでGridBoardPlayerComponent::GetPhase()がkExecutingだったかどうか。
	// AdvanceTurnIfExecutionFinishedが「ちょうど今フレームで実行フェーズが終わった」ことを
	// 検知するための前フレーム比較に使う
	bool wasExecutingLastFrame_ = false;

	// 現在のターン（計画→実行の1サイクル）で取得された（triggered==trueになった）アイテムを、
	// 取得した順に保持する非所有ポインタ一覧。UpdateCollectedItemsDisplayが追加し、
	// FinalizeCollectedItemsOnTurnEndが実際の盤面再配置後にクリアする
	std::vector<GameObject*> collectedItemsThisTurn_;

	// 毎フレーム呼ぶ。SceneBase::Render内のcolliderSystem_.ResolveAndDraw（このメソッドより前に
	// 実行される）がPlayer/Itemの重なりを検知してGridItemComponent::OnTriggerEnterを発火させ、
	// 効果適用済みのアイテムはtriggered=trueになっている。ここでは新たにtriggered==trueになった
	// アイテムをcollectedItemsThisTurn_へ取得順に追加し、リスト内の全アイテムを
	// GridItemSpawnComponent::collectedDisplayTop（プレイヤーの取得済み表示、Inspectorで調整可能）
	// を基準に上から下へ積み重ねて表示する（実際に盤面へ戻すのはFinalizeCollectedItemsOnTurnEndが
	// ターン終了時にまとめて行う。ここでは表示だけを更新し、col/row・盤面への配置は変えない）
	void UpdateCollectedItemsDisplay();

	// AdvanceTurnIfExecutionFinishedが実行フェーズ終了を検知した瞬間に呼ぶ。
	// collectedItemsThisTurn_内の各アイテムへ、現在の空きマスからランダムに選んだcol/rowを
	// 書き換えてtriggeredをfalseへ戻す（＝取得済み表示から盤面へ戻す「リセット」演出）。
	// 処理後、collectedItemsThisTurn_を空にして次のターンに備える
	void FinalizeCollectedItemsOnTurnEnd();

	// 毎フレーム呼ぶ。シーン内にtag==kGridItemTagが1つも存在しなければ（起動直後）、
	// GridItemSpawnComponent::spawnEntriesを読み、各エントリのcount個ぶんをランダムな空きマスへ
	// 配置する（初回配置専用。2回目以降はFinalizeCollectedItemsOnTurnEndがターン終了ごとに
	// 個別に場所を変えるため出番がない）。生成したアイテムはスポナーGameObject
	// （tag==kGridItemSpawnerTag）の子にする
	void RespawnItemsIfNoneExist();

	// 毎フレーム呼ぶ。GridItemSpawnComponent::ConsumeResetRequested()（Inspectorの「リセット」
	// ボタン）がtrueを返した瞬間、既存のtag==kGridItemTagを全部削除してから、SpawnItemsFromConfigで
	// spawnEntries通りに新しく配置し直す（RespawnItemsIfNoneExistと違い、既にアイテムが
	// 存在していても強制的に作り直す）
	void ResetItemsIfRequested();

	// spawner（GridItemSpawnComponent付き）のspawnEntriesを読み、種別ごとにcount個ぶんを現在の
	// 空きマスからランダムに抽選して生成する実処理。RespawnItemsIfNoneExist（初回のみ）と
	// ResetItemsIfRequested（リセットボタン、既存削除後）の両方から呼ばれる共通ロジック
	void SpawnItemsFromConfig(GameObject& spawner, class GridItemSpawnComponent& spawnConfig, class GridBoardComponent& boardSize);

	// 毎フレーム呼ぶ。シーン内にtag==kGridWallTagが1つも存在しなければ（起動直後）、
	// GridWallSpawnComponent::pieceCountぶんの壁ブロック（テトロミノ形、4マス連結）をランダムな
	// 形状・向き・位置で配置する（初回配置専用。RespawnItemsIfNoneExistと同じ理由で、
	// EnsureInitialObjectsExist（初回フレームのみ）とは別の関数にしてある。アイテムと違い壁は
	// 踏んでも消えないため、通常はここで一度配置したら「リセット」ボタンを押すまでそのまま居座り続ける）
	void RespawnWallsIfNoneExist();

	// 毎フレーム呼ぶ。GridWallSpawnComponent::ConsumeResetRequested()（Inspectorの「リセット」
	// ボタン）がtrueを返した瞬間、既存のtag==kGridWallTagを全部削除してから、SpawnWallsFromConfigで
	// pieceCount個ぶんの壁ブロックを新しく配置し直す（ResetItemsIfRequestedと同じ、既存削除後の強制再配置）
	void ResetWallsIfRequested();

	// spawner（GridWallSpawnComponent付き）のpieceCount/passCost/wallColorを読み、テトロミノ7種
	// （I/O/T/S/Z/J/L、ランダムな向きに回転）からランダムに1つ選んで現在の空きマスへ配置する処理を
	// pieceCount回繰り返す実処理（1回につきGridWallComponent付きGameObjectが4個生成される）。
	// SpawnItemsFromConfigの壁版（形状抽選が入る分、単純な1マスずつの抽選ではない）
	void SpawnWallsFromConfig(GameObject& spawner, class GridWallSpawnComponent& spawnConfig, class GridBoardComponent& boardSize);

	// 毎フレーム呼ぶ。tag==kGridWallTagの各GameObjectについて、GridWallComponent::color
	// （Inspectorで調整可能）を兄弟のCubeRenderComponent::colorへ、col/row（配置マス座標）を
	// GridBoardComponent::GridToWorld経由でTransform.translationへ同期する。SyncItemsの壁版
	// （壁にはtriggered相当の「取得済み」状態が無いため、そちらより単純）
	void SyncWalls();

	// FinalizeCollectedItemsOnTurnEnd/RespawnItemsIfNoneExist/ResetItemsIfRequested/
	// RespawnWallsIfNoneExist/ResetWallsIfRequestedの空きマス抽選に使う乱数生成器
	std::mt19937 rng_{ std::random_device{}() };

	// 現在プレイヤーがいるマス・既存の（triggeredでない）アイテムが置かれているマス・既存の壁が
	// 置かれているマスの一覧を返す。アイテムと壁は互いのスポーン抽選母集団からも除外し合う
	// （同じマスに重ねて生成されないようにするため）。RespawnItemsIfNoneExist/
	// FinalizeCollectedItemsOnTurnEnd/RespawnWallsIfNoneExistが空きマス抽選の母集団を作るのに
	// 共通で使う
	std::vector<std::pair<int, int>> ComputeOccupiedCells(class GridBoardComponent* boardSize);

	// boardSize->columns×rowsの全マスから、occupiedに含まれるものを除いた空きマス一覧を返す
	std::vector<std::pair<int, int>> ComputeFreeCells(class GridBoardComponent* boardSize, const std::vector<std::pair<int, int>>& occupied);

	// 毎フレーム呼ぶ。tag==kGridItemTagの各GameObjectについて、GridItemComponent::color
	// （Inspectorで調整可能）を兄弟のCubeRenderComponent::colorへ、col/row（配置マス座標）を
	// GridBoardComponent::GridToWorld経由でTransform.translationへ同期する。アイテムの色・
	// 位置はGridItemComponent側（col/row・color）が真の値で、実際の見た目（CubeRenderComponent::
	// color・Transform.translation）は常にそれに追従させる。Inspectorで手動でGridItemComponentを
	// Add Componentしてcol/rowを入力するだけで、対応するマスへ自動的に移動して表示される
	void SyncItems();
};
