#include "GridPuzzleScene.h"
#include "GameTags.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/Component/Render/CubeRenderComponent.h"
#include "../Engine/GameObject/Component/Physics/GridReflexPlayerComponent.h"
#include "../Engine/GameObject/Component/Physics/GridBoardComponent.h"
#include "../Engine/GameObject/Component/Lighting/DirectionalLightComponent.h"
#include <algorithm>
#include <cmath>

namespace {
	// 盤面の初期サイズ（企画書の「5×15マスの縦長グリッド」）。GridBoardComponentのデフォルト値と
	// 同じにしておく。以降はGridBoardComponent::columns/rows側が真の値で、この定数は
	// EnsureInitialObjectsExistが盤面を初めて生成する瞬間だけ使う
	constexpr int kInitialColumns = 5;
	constexpr int kInitialRows = 15;

	// マス目の中心間隔（ワールド単位）。盤面の列数・行数とは異なり、プレイヤー側
	// （GridReflexPlayerComponent::cellSpacing）と揃えたい見た目の値のため、
	// プレイヤー生成時に一度だけ渡す
	constexpr float kCellSpacing = 1.5f;

	// タイル・プレイヤーの見た目のサイズ（マス間隔よりわずかに小さくして目地の隙間を作る）
	constexpr float kTileSize = kCellSpacing * 0.9f;
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

	// カメラの固定Y座標・Z座標、および初期表示時のX座標（初期盤面の中心）。
	// UpdateCameraFollowが毎フレームX座標だけをこの値からプレイヤー追従で動かす
	constexpr float kCameraCenterX = (kInitialColumns - 1) * 0.5f * kCellSpacing;
	constexpr float kCameraCenterY = -(kInitialRows - 1) * 0.5f * kCellSpacing;
	constexpr float kCameraZ = -30.0f;

	// カメラのX追従の指数減衰係数（CameraFollowComponent::followLerpと同じ式）。
	// 大きいほど素早く追いつく
	constexpr float kCameraFollowLerp = 5.0f;
}

void GridPuzzleScene::OnInitialize() {
	// このシーン専用のカメラ設定。REFLEXと同じくX-Y平面上でゲームが進行する擬似2D
	// （Z座標は固定、カメラは奥から正面を見る）ため、盤面全体（初期15行ぶん）が収まるよう
	// 十分に引いた位置へ固定する。カメラはシーン間で共有されるGameObject非依存の存在なので、
	// 他シーンの操作（マウスドラッグ等）で動いていても、このシーンを開くたびに必ずここへ戻す。
	// タイル・プレイヤーの座標は「マス(0,0)が常にワールド原点」の固定アンカー方式（GridPuzzleScene::
	// RebuildTiles、GridReflexPlayerComponent::GridToWorld参照）のため、盤面は原点から右下方向へ
	// 広がる。初期サイズ(kInitialColumns×kInitialRows)の中心が画面中央に来るよう、カメラのX/Yを
	// その中心座標へオフセットしておく。Y座標は固定のまま（UpdateCameraFollowが上下には動かさない）、
	// X座標だけを毎フレームプレイヤーへなめらかに追従させる
	cameraFollowX_ = kCameraCenterX;
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

	// 計画フェーズ中にクリックできるマスを毎フレーム塗り直す。isPlaying_を問わず呼んで良い
	// （Stop中は単に直近の状態のまま表示され続けるだけで、実害は無い）
	UpdateTileHighlights();

	// カメラのX座標をプレイヤーへなめらかに追従させる（Y/Zは固定）
	UpdateCameraFollow();

	// ESCでいつでもTitleへ戻れるようにする（他シーンと同じキー）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = "Title";
}

void GridPuzzleScene::EnsureInitialObjectsExist() {
	// 盤面フォルダ（見た目・当たり判定を持たない空のGameObject）。PlayScene::GetOrCreateGroupFolderと
	// 同じ「ヒエラルキーをフラットに埋めない」ための工夫。GridBoardComponentが列数・行数・タイル色の
	// 設定値を持つ。scene.jsonから既に読み込まれていれば（＝保存済みの設定があれば）何もしない
	if (!FindObjectByTag(kGridBoardFolderTag)) {
		GameObject& board = CreateObject("Board");
		board.tag = kGridBoardFolderTag;
		board.excludeFromPicking = true;
		auto* boardSize = board.AddComponent<GridBoardComponent>();
		boardSize->columns = kInitialColumns;
		boardSize->rows = kInitialRows;
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

	// プレイヤー。移動ロジックはGridReflexPlayerComponent（ReflexPlayerComponentを継承し、
	// クリック位置の妥当性判定だけをグリッド制約でオーバーライドしたもの）側に閉じており、
	// 盤面（GridBoardComponent）のcolumns/rowsとは意図的に同期させない（互いに独立して
	// Inspectorから調整できればよい）。GridToWorldはgridWidth/gridHeightに依存しない絶対座標
	// （マス(0,0)が常にワールド原点）のため、初期位置は明示的に「初期盤面の中央マス」の
	// ワールド座標へ置く（以前は既定値{0,0}が中央と一致する中央寄せ方式だったが、そちらを
	// 廃止したため、ここで明示的に計算する必要がある）
	if (!FindObjectByTag(GameTags::kPlayer)) {
		GameObject& player = CreateObject("Player");
		player.tag = GameTags::kPlayer;
		player.GetTransform().scale = { kPlayerSize, kPlayerSize, kPlayerSize };
		player.GetTransform().translation.x = (kInitialColumns / 2) * kCellSpacing;
		player.GetTransform().translation.y = -(kInitialRows / 2) * kCellSpacing;
		player.GetTransform().translation.z = kPlayerZOffset;

		auto* playerRender = player.AddComponent<CubeRenderComponent>();
		playerRender->color = kPlayerColor;
		playerRender->lighting = false;

		player.AddComponent<GridReflexPlayerComponent>()->cellSpacing = kCellSpacing;
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

			// マス(0,0)は常にワールド原点（固定アンカー）。GridReflexPlayerComponent::GridToWorldと
			// 必ず同じ式にする（columns/rowsに応じて中央寄せするとプレイヤー側の絶対座標とズレる）
			Transform& t = tile.GetTransform();
			t.translation.x = static_cast<float>(col) * kCellSpacing;
			t.translation.y = -static_cast<float>(row) * kCellSpacing;
			t.translation.z = 0.0f;
			t.scale = { kTileSize, kTileSize, kTileThickness };

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
	if (columns == lastBoardColumns_ && rows == lastBoardRows_) return;

	RebuildTiles(columns, rows);
	lastBoardColumns_ = columns;
	lastBoardRows_ = rows;
}

void GridPuzzleScene::AdvanceTurnIfExecutionFinished() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	auto* playerMove = player ? player->GetComponent<GridReflexPlayerComponent>() : nullptr;
	if (!playerMove) return;

	// ConsumeExecutionFinished()は実行フェーズが完了してPhase::kPreparingに落ちた瞬間だけtrueを
	// 返すワンショットのフラグ（PlayScene::HandleSceneTransitionInputと同じ使い方）。
	// PlayScene側は敵の補充スポーンを待ってからFinishPreparing()を呼ぶが、このシーンには
	// 敵が居ないため、検知した瞬間に即座に計画フェーズへ戻す
	if (playerMove->ConsumeExecutionFinished()) {
		playerMove->FinishPreparing();
	}
}

void GridPuzzleScene::UpdateCameraFollow() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	if (!player) return;

	// CameraFollowComponent::Updateと同じ、フレームレートに依存しない指数減衰補間。
	// Y/Zはここでは一切書き換えず、常にkCameraCenterY/kCameraZのまま固定する
	float targetX = player->GetTransform().translation.x;
	float t = 1.0f - std::exp(-kCameraFollowLerp * lastDeltaTime_);
	cameraFollowX_ += (targetX - cameraFollowX_) * t;

	camera_->SetPosition({ cameraFollowX_, kCameraCenterY, kCameraZ });
}

void GridPuzzleScene::UpdateTileHighlights() {
	// RebuildTiles完了前（起動直後の1フレーム）はタイルが無いため何もしない
	if (tileObjects_.empty() || lastBoardColumns_ <= 0 || lastBoardRows_ <= 0) return;

	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!boardSize) return;

	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	auto* playerMove = player ? player->GetComponent<GridReflexPlayerComponent>() : nullptr;

	// 計画フェーズ以外（実行準備中・実行中）はGetValidTargets()が空を返すため、
	// 自然と全タイルが市松模様の基本色に戻る。ハイライト色自体はplayerMove->highlightColor
	// （InspectorのGridReflexPlayerから調整可能）を都度参照する。GridReflexPlayerComponentは
	// 自分専用の位置状態を持たないため、オーナーの現在Transformを引数で渡す
	std::vector<std::pair<int, int>> validTargets = playerMove ? playerMove->GetValidTargets(player->GetTransform()) : std::vector<std::pair<int, int>>{};
	Vector4 highlightColor = playerMove ? playerMove->highlightColor : boardSize->tileColorA;

	for (int row = 0; row < lastBoardRows_; ++row) {
		for (int col = 0; col < lastBoardColumns_; ++col) {
			GameObject* tile = tileObjects_[static_cast<size_t>(row) * lastBoardColumns_ + col];
			if (!tile) continue;
			auto* render = tile->GetComponent<CubeRenderComponent>();
			if (!render) continue;

			bool isValidTarget = false;
			for (const auto& target : validTargets) {
				if (target.first == col && target.second == row) { isValidTarget = true; break; }
			}
			render->color = isValidTarget ? highlightColor : (((row + col) % 2 == 0) ? boardSize->tileColorA : boardSize->tileColorB);
		}
	}
}

REGISTER_SCENE(GridPuzzleScene, "GridPuzzle");
