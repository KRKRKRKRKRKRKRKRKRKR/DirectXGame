#include "GridPuzzleScene.h"
#include "GameTags.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/Component/Render/CubeRenderComponent.h"
#include "../Engine/GameObject/Component/Physics/GridBoardComponent.h"
#include "../Engine/GameObject/Component/Physics/GridBoardPlayerComponent.h"
#include "../Engine/GameObject/Component/Physics/GridItemComponent.h"
#include "../Engine/GameObject/Component/Lighting/DirectionalLightComponent.h"
#include <algorithm>

namespace {
	// 盤面の初期サイズ（企画書の「5×15マスの縦長グリッド」）。GridBoardComponentのデフォルト値と
	// 同じにしておく。以降はGridBoardComponent::columns/rows側が真の値で、この定数は
	// EnsureInitialObjectsExistが盤面を初めて生成する瞬間だけ使う
	constexpr int kInitialColumns = 5;
	constexpr int kInitialRows = 15;

	// マス目の中心間隔（ワールド単位）の初期値。EnsureInitialObjectsExistが盤面を初めて生成する
	// 瞬間にGridBoardComponent::cellSpacingへ一度だけ書き込む（以降はそちらが唯一の値）。
	// カメラの初期中心位置計算にも使う
	constexpr float kCellSpacing = 1.5f;

	// タイルの厚み・プレイヤーの見た目サイズ（マス間隔に対する比率）。タイルの平面サイズ自体は
	// RebuildTilesがGridBoardComponent::cellSpacingから毎回動的に計算する
	constexpr float kTileThickness = 0.3f;
	constexpr float kPlayerSize = kCellSpacing * 0.6f;

	// プレイヤーをタイルより少しカメラ側（Z-方向）に置き、重なっても手前に見えるようにする
	constexpr float kPlayerZOffset = -0.5f;

	// タイルはlighting=trueのため、DirectionalLightComponentが無いと真っ黒になってしまう
	// （EnsureInitialObjectsExistで光源を1つ置く）
	constexpr Vector4 kPlayerColor = { 0.95f, 0.35f, 0.20f, 1.0f };

	constexpr const char* kGridCellTag = "GridCell";
	constexpr const char* kGridBoardFolderTag = "GridBoard";
	constexpr const char* kGridLightTag = "GridPuzzleLight";
	constexpr const char* kGridItemTag = "GridItem";

	// アイテムの見た目サイズ（マス間隔に対する比率）・色（種別ごと）。プレイヤーと同じく
	// タイルより少しカメラ側に置く
	constexpr float kItemSize = kCellSpacing * 0.4f;
	constexpr float kItemZOffset = -0.3f;
	constexpr Vector4 kItemColorAttackPower = { 0.9f, 0.2f, 0.2f, 1.0f };  // 赤：攻撃力+1
	constexpr Vector4 kItemColorCostFixed = { 0.2f, 0.5f, 0.95f, 1.0f };   // 青：コスト+2固定
	constexpr Vector4 kItemColorCostRisky = { 0.85f, 0.55f, 0.95f, 1.0f }; // 紫：コスト±4リスキー

	// アイテムの固定配置（列,行）。盤面リセット時のランダム再配置は未実装のため、起動時に
	// 一度だけこの座標へ配置する（初期盤面5列×15行、プレイヤー初期位置は中央(2,7)を避ける）
	struct ItemPlacement { int col; int row; };
	constexpr ItemPlacement kInitialItemPlacements[] = {
		{ 1, 3 },  // 攻撃力+1
		{ 3, 7 },  // コスト+2固定
		{ 1, 11 }, // コスト±4リスキー
	};

	// カメラの固定位置（初期盤面の中心を画面中央に収める）。追従は行わず常にこの位置のまま
	constexpr float kCameraCenterX = (kInitialColumns - 1) * 0.5f * kCellSpacing;
	constexpr float kCameraCenterY = -(kInitialRows - 1) * 0.5f * kCellSpacing;
	constexpr float kCameraZ = -30.0f;
}

void GridPuzzleScene::OnInitialize() {
	// このシーン専用のカメラ設定。REFLEXと同じくX-Y平面上でゲームが進行する擬似2D
	// （Z座標は固定、カメラは奥から正面を見る）ため、盤面全体（初期15行ぶん）が収まるよう
	// 十分に引いた位置へ固定する。カメラはシーン間で共有されるGameObject非依存の存在なので、
	// 他シーンの操作（マウスドラッグ等）で動いていても、このシーンを開くたびに必ずここへ戻す。
	// タイル・プレイヤーの座標は「マス(0,0)が常にワールド原点」の固定アンカー方式
	// （GridBoardComponent::GridToWorld参照）のため、盤面は原点から右下方向へ広がる。
	// 初期サイズ(kInitialColumns×kInitialRows)の中心が画面中央に来るよう、カメラのX/Yを
	// その中心座標へ固定する（追従はしない）
	camera_->SetPosition({ kCameraCenterX, kCameraCenterY, kCameraZ });
	camera_->SetRotation({ 0.0f, 0.0f, 0.0f });
	camera_->SetFov(45.0f);
}

void GridPuzzleScene::HandleSceneTransitionInput() {
	// 初回フレームのみ：盤面・プレイヤー・光源のうち無いものだけを組み立てる。OnInitialize()は
	// LoadScene()より前に呼ばれるため、そこで判定すると保存データがまだ読み込まれておらず
	// 毎回「無い」と誤判定してしまう。TutorialScene::needsInitialSpawn_と同じ理由で、
	// LoadScene()完了後に必ず呼ばれるこのタイミングまで待つ
	if (needsInitialSpawn_) {
		needsInitialSpawn_ = false;
		EnsureInitialObjectsExist();
	}

	// GridBoardComponentの列数・行数がInspectorで変更されていたら、タイルだけを作り直す
	// （プレイヤーには一切触れない）。タイル自体はexcludeFromSave=trueで保存されないため、
	// 起動直後（lastBoardColumns_/Rows_が初期値0のまま）は必ずここで初めて生成される
	RebuildTilesIfBoardSizeChanged();

	// 実行フェーズが終わってPhase::kPreparingに落ちたままになっていたら、即座に計画フェーズへ戻す
	// （このシーンには敵が居らず、待つ理由が無いため）
	AdvanceTurnIfExecutionFinished();

	// プレイヤーが直前のフレームで新たに拾った（発動した）アイテムを盤面から削除する
	ProcessTriggeredItems();

	// Inspectorで変更されたGridItemComponent::colorを、兄弟のCubeRenderComponent::colorへ反映する
	SyncItemColors();

	// 計画フェーズ中にクリックできるマスを毎フレーム塗り直す。isPlaying_を問わず呼んで良い
	// （Stop中は単に直近の状態のまま表示され続けるだけで、実害は無い）
	UpdateTileHighlights();

	// ESCでいつでもTitleへ戻れるようにする（他シーンと同じキー）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = "Title";
}

void GridPuzzleScene::EnsureInitialObjectsExist() {
	// 盤面フォルダ（見た目・当たり判定を持たない空のGameObject）。PlayScene::GetOrCreateGroupFolderと
	// 同じ「ヒエラルキーをフラットに埋めない」ための工夫。GridBoardComponentが列数・行数・タイル色の
	// 設定値を持つ。scene.jsonから既に読み込まれていれば（＝保存済みの設定があれば）何もしない
	bool isBoardNewlyCreated = !FindObjectByTag(kGridBoardFolderTag);
	if (isBoardNewlyCreated) {
		GameObject& board = CreateObject("Board");
		board.tag = kGridBoardFolderTag;
		board.excludeFromPicking = true;
		auto* boardSize = board.AddComponent<GridBoardComponent>();
		boardSize->columns = kInitialColumns;
		boardSize->rows = kInitialRows;
		boardSize->cellSpacing = kCellSpacing;
	}

	// タイルの色をlighting=trueで正しく見せるための平行光源。DirectionalLightComponentが
	// 1つも無いとSceneLightのenableDirectionalが0のままになり、lighting有効のオブジェクトは
	// 真っ黒に描画されてしまう（HLSL側のdiffuse加算対象が無いため）。
	// 以前のバージョンではtagを設定せず名前（name=="GridPuzzleLight"）だけで存在判定していたため、
	// それ以前に保存されたscene.jsonにはtagが空のまま残っている場合がある。tag一致に加えて
	// 名前一致でも探すことで、そうした旧データを正しく1個の既存オブジェクトとして認識する
	// （＝tag一致が見つからず毎回新規生成してしまい、光源が際限なく増え続ける不具合を防ぐ）。
	// 何らかの理由で既に複数存在してしまっていた場合は、2個目以降を削除して1個に統合する
	std::vector<GameObject*> lightCandidates;
	for (auto& obj : objects_) {
		if (obj->tag == kGridLightTag || obj->name == "GridPuzzleLight") lightCandidates.push_back(obj.get());
	}
	if (lightCandidates.empty()) {
		GameObject& light = CreateObject("GridPuzzleLight");
		light.tag = kGridLightTag;
		light.AddComponent<DirectionalLightComponent>();
	} else {
		lightCandidates.front()->tag = kGridLightTag; // 旧データ（tag未設定）を補正する
		if (lightCandidates.size() > 1) {
			std::vector<GameObject*> duplicateLights(lightCandidates.begin() + 1, lightCandidates.end());
			DeleteObjects(duplicateLights);
		}
	}

	// プレイヤー。移動ロジックはGridBoardPlayerComponent（ReflexPlayerComponentに依存しない
	// 独立実装。同じ行/列上のマスをクリックして経路予約→実行フェーズで移動、コスト制）側に
	// 閉じている。盤面サイズ・マス間隔はGridBoardComponent（唯一のデータソース）から取得し、
	// 初期位置はGridToWorld経由で「初期盤面の中央マス」のワールド座標へ置く（手書きの複製式にしない）
	if (!FindObjectByTag(GameTags::kPlayer)) {
		GameObject* board = FindObjectByTag(kGridBoardFolderTag);
		auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;

		GameObject& player = CreateObject("Player");
		player.tag = GameTags::kPlayer;
		player.GetTransform().scale = { kPlayerSize, kPlayerSize, kPlayerSize };
		if (boardSize) {
			Vector3 centerPos = boardSize->GridToWorld(boardSize->columns / 2, boardSize->rows / 2);
			player.GetTransform().translation.x = centerPos.x;
			player.GetTransform().translation.y = centerPos.y;
		}
		player.GetTransform().translation.z = kPlayerZOffset;

		auto* playerRender = player.AddComponent<CubeRenderComponent>();
		playerRender->color = kPlayerColor;
		playerRender->lighting = false;

		player.AddComponent<GridBoardPlayerComponent>();
	}

	// アイテム（GridItemComponent、攻撃力+1／コスト+2固定／コスト±4リスキーの3種）。
	// 盤面リセット時のランダム再配置は未実装のため、起動時に固定座標(kInitialItemPlacements)へ
	// 一度だけ配置する。判定は「盤面フォルダが今回新規生成されたか」（＝scene.jsonに保存データが
	// 無かった、完全な初回起動）で行う。GridBoardFolderと同時に必ず生成されるため、2回目以降の
	// 起動（拾われて0個になっている場合を含む）では再配置しない
	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (boardSize && isBoardNewlyCreated) {
		auto spawnItem = [&](GridItemComponent::Type type, int col, int row, const Vector4& color) {
			GameObject& item = CreateObject("Item");
			item.tag = kGridItemTag;
			item.GetTransform().scale = { kItemSize, kItemSize, kItemSize };
			Vector3 pos = boardSize->GridToWorld(col, row);
			item.GetTransform().translation = { pos.x, pos.y, kItemZOffset };

			auto* render = item.AddComponent<CubeRenderComponent>();
			render->color = color;
			render->lighting = false;

			auto* itemComp = item.AddComponent<GridItemComponent>();
			itemComp->type = type;
			itemComp->col = col;
			itemComp->row = row;
			itemComp->color = color; // Inspectorで調整する色の初期値（種別ごとの既定色）
			};

		spawnItem(GridItemComponent::Type::kAttackPower, kInitialItemPlacements[0].col, kInitialItemPlacements[0].row, kItemColorAttackPower);
		spawnItem(GridItemComponent::Type::kCostFixed, kInitialItemPlacements[1].col, kInitialItemPlacements[1].row, kItemColorCostFixed);
		spawnItem(GridItemComponent::Type::kCostRisky, kInitialItemPlacements[2].col, kInitialItemPlacements[2].row, kItemColorCostRisky);
	}

	// CreateObjectで追加したGameObjectをgizmoTargets_（Update/Draw対象一覧）に反映する
	RebuildDerivedLists();
}

void GridPuzzleScene::RebuildTiles(int columns, int rows) {
	columns = (std::max)(columns, 1);
	rows = (std::max)(rows, 1);

	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!board || !boardSize) return;

	// 既存タイルだけを全部消す（盤面フォルダ自体・プレイヤー・光源には触れない）
	std::vector<GameObject*> existingTiles;
	for (auto& obj : objects_) {
		if (obj->tag == kGridCellTag) existingTiles.push_back(obj.get());
	}
	if (!existingTiles.empty()) DeleteObjects(existingTiles);

	// UpdateTileHighlightsがrow*columns+colで引けるよう、生成順を保ったまま詰め直す
	tileObjects_.assign(static_cast<size_t>(columns) * rows, nullptr);

	for (int row = 0; row < rows; ++row) {
		for (int col = 0; col < columns; ++col) {
			GameObject& tile = CreateObject("Tile");
			tile.tag = kGridCellTag;
			tile.SetParent(board);

			// タイルは常にGridBoardComponentの設定から毎回作り直す一時的な見た目のため、
			// scene.jsonへは保存しない（保存してしまうと次回起動時に古い枚数のまま重複生成される）
			tile.excludeFromSave = true;

			// マス(0,0)は常にワールド原点（固定アンカー）。GridBoardComponent::GridToWorld
			// （盤面仕様の唯一のデータソース）を通して計算するため、プレイヤー側の移動判定
			// （専用コンポーネント実装後も同じGridBoardComponentを参照する想定）と常に一致する
			Transform& t = tile.GetTransform();
			t.translation = boardSize->GridToWorld(col, row);
			// タイルの見た目サイズもboardSize->cellSpacing基準（マス間隔よりわずかに小さくして
			// 目地の隙間を作る）。Inspectorでマス間隔を変えるとタイルサイズもその場で追従する
			float tileSize = boardSize->cellSpacing * 0.9f;
			t.scale = { tileSize, tileSize, kTileThickness };

			auto* render = tile.AddComponent<CubeRenderComponent>();
			render->color = ((row + col) % 2 == 0) ? boardSize->tileColorA : boardSize->tileColorB;
			render->lighting = true;

			tileObjects_[static_cast<size_t>(row) * columns + col] = &tile;
		}
	}

	RebuildDerivedLists();
}

void GridPuzzleScene::RebuildTilesIfBoardSizeChanged() {
	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!boardSize) return;

	int columns = (std::max)(boardSize->columns, 1);
	int rows = (std::max)(boardSize->rows, 1);
	if (columns == lastBoardColumns_ && rows == lastBoardRows_ && boardSize->cellSpacing == lastBoardCellSpacing_) return;

	RebuildTiles(columns, rows);
	lastBoardColumns_ = columns;
	lastBoardRows_ = rows;
	lastBoardCellSpacing_ = boardSize->cellSpacing;
}

void GridPuzzleScene::AdvanceTurnIfExecutionFinished() {
	// GridBoardPlayerComponentはReflexPlayerComponentと異なり「実行フェーズ完了→シーン側が
	// 明示的に計画フェーズへ戻す」準備フェーズを持たない。実行フェーズが完了した瞬間に
	// コンポーネント自身が計画フェーズへ自動遷移するため、シーン側で何もする必要が無い
}

void GridPuzzleScene::ProcessTriggeredItems() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	auto* playerMove = player ? player->GetComponent<GridBoardPlayerComponent>() : nullptr;
	if (!playerMove) return;

	std::vector<GameObject*> triggered = playerMove->ConsumeTriggeredItems();
	if (!triggered.empty()) DeleteObjects(triggered);
}

void GridPuzzleScene::SyncItemColors() {
	for (auto& obj : objects_) {
		if (obj->tag != kGridItemTag) continue;
		auto* itemComp = obj->GetComponent<GridItemComponent>();
		auto* render = obj->GetComponent<CubeRenderComponent>();
		if (itemComp && render) render->color = itemComp->color;
	}
}

void GridPuzzleScene::UpdateTileHighlights() {
	// RebuildTiles完了前（起動直後の1フレーム）はタイルが無いため何もしない
	if (tileObjects_.empty() || lastBoardColumns_ <= 0 || lastBoardRows_ <= 0) return;

	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!boardSize) return;

	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	auto* playerMove = player ? player->GetComponent<GridBoardPlayerComponent>() : nullptr;

	// 計画フェーズ以外（実行中）はGetValidTargets()が空を返すため、自然と移動可能マスの
	// ハイライトは消える。ハイライト色・予約色自体はplayerMove側（InspectorのGridBoardPlayerから
	// 調整可能）を都度参照する。GridBoardPlayerComponentは自分専用の位置状態・盤面参照を
	// 持たないため、オーナーの現在Transformとシーン内GameObject一覧（GridBoardComponent検索用）を
	// 引数で渡す。経路の可視化は波紋マーカー・破線を使わず、予約済みマス自体をタイルの色塗りで
	// 表現する（reservedColor、GetReservedWaypoints()）
	std::vector<std::pair<int, int>> validTargets = playerMove ? playerMove->GetValidTargets(player->GetTransform(), &gizmoTargets_) : std::vector<std::pair<int, int>>{};
	std::vector<std::pair<int, int>> reservedWaypoints = playerMove ? playerMove->GetReservedWaypoints() : std::vector<std::pair<int, int>>{};
	Vector4 highlightColor = playerMove ? playerMove->highlightColor : boardSize->tileColorA;
	Vector4 reservedColor = playerMove ? playerMove->reservedColor : boardSize->tileColorA;

	for (int row = 0; row < lastBoardRows_; ++row) {
		for (int col = 0; col < lastBoardColumns_; ++col) {
			GameObject* tile = tileObjects_[static_cast<size_t>(row) * lastBoardColumns_ + col];
			if (!tile) continue;
			auto* render = tile->GetComponent<CubeRenderComponent>();
			if (!render) continue;

			bool isReserved = false;
			for (const auto& cell : reservedWaypoints) {
				if (cell.first == col && cell.second == row) { isReserved = true; break; }
			}
			bool isValidTarget = false;
			if (!isReserved) {
				for (const auto& target : validTargets) {
					if (target.first == col && target.second == row) { isValidTarget = true; break; }
				}
			}

			if (isReserved) render->color = reservedColor;
			else if (isValidTarget) render->color = highlightColor;
			else render->color = ((row + col) % 2 == 0) ? boardSize->tileColorA : boardSize->tileColorB;
		}
	}
}

REGISTER_SCENE(GridPuzzleScene, "GridPuzzle");
