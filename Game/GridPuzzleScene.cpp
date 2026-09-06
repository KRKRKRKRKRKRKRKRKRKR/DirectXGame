#include "GridPuzzleScene.h"
#include "GameTags.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/Component/Render/CubeRenderComponent.h"
#include "../Engine/GameObject/Component/Physics/GridBoardComponent.h"
#include "../Engine/GameObject/Component/Physics/GridBoardPlayerComponent.h"
#include "../Engine/GameObject/Component/Physics/GridItemComponent.h"
#include "../Engine/GameObject/Component/Physics/GridItemSpawnComponent.h"
#include "../Engine/GameObject/Component/Physics/GridWallComponent.h"
#include "../Engine/GameObject/Component/Physics/GridWallSpawnComponent.h"
#include "../Engine/GameObject/Component/Physics/OBBColliderComponent.h"
#include "../Engine/GameObject/Component/Lighting/DirectionalLightComponent.h"
#include "../Engine/GameObject/Component/Render/TextSpriteComponent.h"
#include "../Engine/GameObject/Component/Render/AlphabetTextComponent.h"
#include "../Engine/GameObject/Component/Render/EnemyHealthBarComponent.h"
#include "../Engine/Utils/Logger.h"
#include "../Externals/Json/json.hpp"
#include <algorithm>
#include <cstdlib>
#include <format>
#include <fstream>
#include <numbers>
#include <string>

namespace {
	// 盤面サイズは7x7固定（企画上の決定、GridBoardComponent::kFixedColumns/kFixedRowsと同じ値）。
	// EnsureInitialObjectsExistが盤面を初めて生成する瞬間、カメラの初期中心位置計算にも使う
	constexpr int kInitialColumns = GridBoardComponent::kFixedColumns;
	constexpr int kInitialRows = GridBoardComponent::kFixedRows;

	// マス目の中心間隔（ワールド単位）の初期値。EnsureInitialObjectsExistが盤面を初めて生成する
	// 瞬間にGridBoardComponent::cellSpacingへ一度だけ書き込む（以降はそちらが唯一の値）。
	// カメラの初期中心位置計算にも使う
	constexpr float kCellSpacing = 1.5f;

	// タイルの厚み・プレイヤーの見た目サイズ（マス間隔に対する比率）。タイルの平面サイズ自体は
	// RebuildTilesがGridBoardComponent::cellSpacingから毎回動的に計算する
	constexpr float kTileThickness = 0.3f;
	constexpr float kPlayerSize = kCellSpacing * 0.6f;

	// 盤面はX-Z平面（水平な地面、Y=0）に広がる見下ろし視点のため、プレイヤーは地面より
	// 少し高い位置（+Y）に置いて重なっても地面より手前（上）に見えるようにする
	constexpr float kPlayerHeightOffset = 0.5f;

	// タイルはlighting=trueのため、DirectionalLightComponentが無いと真っ黒になってしまう
	// （EnsureInitialObjectsExistで光源を1つ置く）
	constexpr Vector4 kPlayerColor = { 0.95f, 0.35f, 0.20f, 1.0f };

	constexpr const char* kGridCellTag = "GridCell";
	constexpr const char* kGridBoardFolderTag = "GridBoard";
	constexpr const char* kGridLightTag = "GridPuzzleLight";
	constexpr const char* kGridItemTag = "GridItem";
	constexpr const char* kGridItemSpawnerTag = "GridItemSpawner";
	constexpr const char* kGridWallTag = "GridWall";
	constexpr const char* kGridWallSpawnerTag = "GridWallSpawner";
	constexpr const char* kGridCostTextTag = "GridCostText";
	constexpr const char* kGridEnemyHealthBarTag = "GridEnemyHealthBar";
	constexpr const char* kGridLifeHealthBarTag = "GridLifeHealthBar";
	constexpr const char* kGridHealthBarLabelTag = "GridHealthBarLabel";

	// 敵HPバーを赤/緑/青の3本、独立して並べる。各バーはEnemyHealthBarComponent::watchedTypesで
	// 対応する1種別だけを監視する（他の2種別の取得では減らない）。GridItemComponent::Typeの
	// 並び（kAttackPower=0が赤、kCostFixed=1が緑、kCostRisky=2が青。Inspector表示名も
	// GridItemSpawnComponent::kTypeNames/EnemyHealthBarComponent::kItemTypeNamesで「赤/緑/青」に
	// 変更済み）と対応させるため、この配列もその順番で持つ
	struct EnemyHealthBarSpec {
		GridItemComponent::Type watchedType;
		Vector4 fillColor;
	};
	constexpr Vector4 kRedFillColor = { 0.85f, 0.2f, 0.2f, 1.0f };
	constexpr Vector4 kGreenFillColor = { 0.25f, 0.8f, 0.3f, 1.0f };
	constexpr Vector4 kBlueFillColor = { 0.25f, 0.45f, 0.9f, 1.0f };
	const EnemyHealthBarSpec kEnemyHealthBarSpecs[] = {
		{ GridItemComponent::Type::kAttackPower, kRedFillColor },
		{ GridItemComponent::Type::kCostFixed, kGreenFillColor },
		{ GridItemComponent::Type::kCostRisky, kBlueFillColor },
	};
	constexpr int kEnemyHealthBarCount = 3;

	// ライフバー（赤/緑/青とは別枠、4本目）：1ターンのうちに赤/緑/青の3本を全部空にできなかった
	// 回数を表す。EnemyHealthBarComponentをそのまま流用するが、watchedTypesによるアイテム連動は
	// 使わず、GridPuzzleScene::AdvanceTurnIfExecutionFinishedがNotify1FailureOccurred()を
	// 直接呼んで1目盛りずつ減らす（アイテム種別を問わない「ターン失敗」1回＝1目盛り）
	constexpr Vector4 kLifeFillColor = { 0.9f, 0.9f, 0.85f, 1.0f }; // 白系（赤/緑/青と区別する）
	constexpr int kLifeMaxCount = 3;
	// 表示本数（赤/緑/青＋ライフの4本）。EnemyHealthBarPositionの横並び計算に使う
	constexpr int kTotalHealthBarCount = kEnemyHealthBarCount + 1;

	// アイテムの見た目サイズ（マス間隔に対する比率）。プレイヤーと同じく地面より少し高い位置
	// （+Y）に置く。生成時の色自体はGridItemSpawnComponent::SpawnEntry::colorが持つ
	// （種別ごとの既定色をここに固定で持たない）
	constexpr float kItemSize = kCellSpacing * 0.4f;
	constexpr float kItemHeightOffset = 0.3f;

	// 壁の見た目サイズ（マス間隔に対する比率）。アイテムより一回り大きくして、マスをほぼ覆う
	// ように見せる（見下ろし視点のため高さでの区別はほぼ付かず、色とサイズで見分ける）
	constexpr float kWallSize = kCellSpacing * 0.85f;
	constexpr float kWallHeightOffset = 0.35f;

	// カメラの固定位置（初期盤面の中心を画面中央に収める）。追従は行わず常にこの位置のまま。
	// 盤面はX-Z平面上（GridBoardComponent::GridToWorld参照）に広がるため、カメラは盤面の
	// 真上（+Y）から真下（-Y方向）を見下ろす姿勢にする
	constexpr float kCameraCenterX = (kInitialColumns - 1) * 0.5f * kCellSpacing;
	constexpr float kCameraCenterZ = -(kInitialRows - 1) * 0.5f * kCellSpacing;
	constexpr float kCameraHeight = 30.0f;

	// 敵HPバー（EnemyHealthBarComponent、ワールド空間の3Dオブジェクト）の見た目サイズと配置座標。
	// 盤面はマス(0,0)がワールド原点でX+・Z-方向へ広がる見下ろし視点のため、「盤面の上（手前）」
	// ＝row=0より外側のZ+方向に、赤/緑/青＋ライフの4本を横（X方向）に並べる
	constexpr float kEnemyHealthBarWidth = kInitialColumns * kCellSpacing * 0.2f;
	constexpr float kEnemyHealthBarHeight = 0.4f;
	constexpr float kEnemyHealthBarGap = kEnemyHealthBarWidth * 1.2f; // 隣のバーとの中心間隔
	constexpr float kEnemyHealthBarZ = kCellSpacing * 1.5f;

	// 赤/緑/青バーの上に表示する「残量/最大値」ラベル（AlphabetTextComponent）のローカル配置。
	// 親（各バーGameObject）からの相対オフセット（Z+方向＝盤面から見てさらに奥、カメラ上方）
	constexpr float kHealthBarLabelZOffset = 0.5f;
	constexpr float kHealthBarLabelCharScale = 0.35f;
	constexpr float kHealthBarLabelCharSpacing = 0.4f;

	// index番目（0=赤,1=緑,2=青,3=ライフ）のバーの配置座標。kTotalHealthBarCount本の中心が
	// 盤面X中心(kCameraCenterX)に来るよう、左右対称に並べる
	Vector3 EnemyHealthBarPosition(int index) {
		float offsetX = (static_cast<float>(index) - (kTotalHealthBarCount - 1) * 0.5f) * kEnemyHealthBarGap;
		return { kCameraCenterX + offsetX, 0.5f, kEnemyHealthBarZ };
	}

	// 壁の手動デザイン基本形一覧を保存しているJSONファイルのパス。実行ファイルからの相対パスで
	// 開く（RankingManager::kRankingPathと同じ方式）。各パターンは"cells"に[col,row]の配列を持つ
	// （WallPatterns.json参照）。新しいパターンを追加したい場合はこのファイルを編集するだけでよく、
	// ビルドし直す必要はない
	constexpr const char* kWallPatternsPath = "Resources/GridPuzzle/WallPatterns.json";

	// 壁の手動デザイン基本形一覧（各要素が(col,row)一覧、7x7盤面専用）。kWallPatternsPathから
	// 読み込む。ユーザーが手動でデザインしたレイアウトで、外周・四隅への接触制約は設けていない
	// （基本形によっては外周列・行に接触するものもあるが、いずれも意図した配置）。
	// FixedWallPatternVariants()が基本形それぞれを回転・反転してバリエーションを増やすための
	// 元データ。ファイルが存在しない・壊れている場合は空を返す（＝壁が1枚も生成されなくなる。
	// 起動時に必ずWallPatterns.jsonを配置しておく運用を前提とする）
	const std::vector<std::vector<std::pair<int, int>>>& WallPatternBases() {
		static const std::vector<std::vector<std::pair<int, int>>> kBases = [] {
			std::vector<std::vector<std::pair<int, int>>> bases;

			std::ifstream in(kWallPatternsPath, std::ios::binary);
			if (!in.is_open()) {
				Logger::Log(std::format("GridPuzzleScene: 壁パターンファイル '{}' を開けませんでした\n", kWallPatternsPath));
				return bases;
			}

			nlohmann::json j;
			try {
				in >> j;
			} catch (const std::exception& e) {
				Logger::Log(std::format("GridPuzzleScene: 壁パターンファイルの解析に失敗しました: {}\n", e.what()));
				return bases;
			}

			if (!j.contains("patterns")) return bases;
			for (const auto& patternJson : j["patterns"]) {
				if (!patternJson.contains("cells")) continue;
				std::vector<std::pair<int, int>> cells;
				for (const auto& cellJson : patternJson["cells"]) {
					if (cellJson.size() < 2) continue;
					cells.push_back({ cellJson[0].get<int>(), cellJson[1].get<int>() });
				}
				if (!cells.empty()) bases.push_back(std::move(cells));
			}
			return bases;
			}();
		return kBases;
	}

	// 盤面の最後尾インデックス（7x7なら6）。90度回転・左右反転の座標変換に使う
	constexpr int kWallPatternLastIndex = (GridBoardComponent::kFixedColumns > GridBoardComponent::kFixedRows
		? GridBoardComponent::kFixedColumns : GridBoardComponent::kFixedRows) - 1;

	// 基本形の1つを90度回転（盤面中心基準、正方形前提でcol/rowを入れ替えて反転するだけで済む）
	std::vector<std::pair<int, int>> RotateWallPattern90(const std::vector<std::pair<int, int>>& cells) {
		std::vector<std::pair<int, int>> rotated;
		rotated.reserve(cells.size());
		for (const auto& cell : cells) rotated.push_back({ kWallPatternLastIndex - cell.second, cell.first });
		return rotated;
	}

	// 基本形の1つを左右反転（col軸を反転、rowはそのまま）
	std::vector<std::pair<int, int>> MirrorWallPattern(const std::vector<std::pair<int, int>>& cells) {
		std::vector<std::pair<int, int>> mirrored;
		mirrored.reserve(cells.size());
		for (const auto& cell : cells) mirrored.push_back({ kWallPatternLastIndex - cell.first, cell.second });
		return mirrored;
	}

	// WallPatternBases()の各基本形を90度回転4通り×左右反転の有無2通りで、最大8種類ずつの
	// バリエーションにかさ増しする（対称な形の場合、重複したバリエーションが含まれることが
	// あるが、盤面から見れば単に同じ形が選ばれる確率がわずかに上がるだけで実害はないため
	// 重複除去はしない）。基本形が2つなら最大16種類になる。GridPuzzleScene::
	// SpawnWallsFromConfigが実行フェーズ終了・リセットのたびにこの中からランダムに1つ選ぶ
	const std::vector<std::vector<std::pair<int, int>>>& FixedWallPatternVariants() {
		static const std::vector<std::vector<std::pair<int, int>>> kVariants = [] {
			std::vector<std::vector<std::pair<int, int>>> variants;
			for (const auto& base : WallPatternBases()) {
				std::vector<std::pair<int, int>> current = base;
				for (int r = 0; r < 4; ++r) {
					variants.push_back(current);
					variants.push_back(MirrorWallPattern(current));
					current = RotateWallPattern90(current);
				}
			}
			return variants;
			}();
		return kVariants;
	}

	// Camera::GetViewMatrixはrotationをXMMatrixRotationRollPitchYaw(pitch=x, yaw=y, roll=z)で
	// 解釈し、rotation={0,0,0}のときの正面方向は+Z。pitch(rotation.x)をちょうど+90度にすると
	// 正面方向が真下(-Y)を向く（真上から真下を見下ろす姿勢になる）
	constexpr float kCameraPitchStraightDown = std::numbers::pi_v<float> * 0.5f;
}

void GridPuzzleScene::OnInitialize() {
	// このシーン専用のカメラ設定。盤面はX-Z平面（水平な地面、Y=0）上に広がるため、カメラは
	// 盤面の真上（+Y）から真下を見下ろす姿勢に固定する。カメラはシーン間で共有される
	// GameObject非依存の存在なので、他シーンの操作（マウスドラッグ等）で動いていても、
	// このシーンを開くたびに必ずここへ戻す。タイル・プレイヤーの座標は「マス(0,0)が常に
	// ワールド原点」の固定アンカー方式（GridBoardComponent::GridToWorld参照）のため、盤面は
	// 原点からX+・Z-方向へ広がる。初期サイズ(kInitialColumns×kInitialRows)の中心が画面中央に
	// 来るよう、カメラのX/Zをその中心座標へ固定する（追従はしない）
	camera_->SetPosition({ kCameraCenterX, kCameraHeight, kCameraCenterZ });
	camera_->SetRotation({ kCameraPitchStraightDown, 0.0f, 0.0f });
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

	// 新たに取得（triggered==true）されたアイテムを取得済み表示リストへ追加し、積み重ね表示する
	UpdateCollectedItemsDisplay();

	// 実行フェーズがちょうど終わった瞬間を検知したら、取得済みアイテムを盤面へ戻す
	// （プレイヤーの行動が全て終わるまでは盤面へ戻さない）
	AdvanceTurnIfExecutionFinished();

	// 壁が1つも無ければ（起動直後）、GridWallSpawnComponentの設定通りに配置する。アイテムより
	// 先に壁を配置することで、アイテム側のスポーン抽選（ComputeOccupiedCells）が壁のマスを
	// 避けられるようにする（壁は固定パターンのため、逆順だとアイテムの上に壁が重なって
	// 配置され、指定した個数のアイテムが実質埋もれてしまう不具合があった）
	RespawnWallsIfNoneExist();

	// Inspectorの「リセット」ボタンが押されていたら、既存の壁を全部消して配置し直す
	ResetWallsIfRequested();

	// アイテムが1つも無ければ（起動直後）、GridItemSpawnComponentの設定通りに配置する
	RespawnItemsIfNoneExist();

	// Inspectorの「リセット」ボタンが押されていたら、既存アイテムを全部消して配置し直す
	ResetItemsIfRequested();

	// Inspectorの「超えられる／超えられないを切り替え」ボタンが押されていたら、
	// 現在盤面上にある全壁のimpassableを一括で書き換える
	ApplyImpassableToggleIfRequested();

	// Inspectorで変更されたGridItemComponent::color/col/rowを、兄弟のCubeRenderComponent::color・
	// Transform.translationへ反映する
	SyncItems();

	// Inspectorで変更されたGridWallComponent::color/col/rowを、兄弟のCubeRenderComponent::color・
	// Transform.translationへ反映する
	SyncWalls();

	// 行動可能マス数（残り移動コスト）のテキスト表示を最新の値に更新する
	UpdateCostText();

	// 赤/緑/青の敵HPバーの上に表示する「残量/最大値」ラベルを最新の値に更新する
	UpdateEnemyHealthBarLabels();

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
			player.GetTransform().translation.z = centerPos.z;
		}
		player.GetTransform().translation.y = kPlayerHeightOffset;

		auto* playerRender = player.AddComponent<CubeRenderComponent>();
		playerRender->color = kPlayerColor;
		playerRender->lighting = false;

		player.AddComponent<GridBoardPlayerComponent>();

		// アイテム取得の当たり判定用。isTrigger=trueで押し戻しは行わず、重なりの検知
		// （ColliderSystem::ResolveAndDraw→OnTriggerEnter）だけに使う
		auto* playerCollider = player.AddComponent<OBBColliderComponent>();
		playerCollider->layer = CollisionLayer::kPlayer;
		playerCollider->isTrigger = true;
		playerCollider->halfSize = { kPlayerSize * 0.5f, kPlayerSize * 0.5f, kPlayerSize * 0.5f };
	}

	// アイテムスポナー（見た目・当たり判定を持たない空のGameObject）。GridItemSpawnComponentが
	// 「どの種類を何個、盤面に存在させ続けるか」の設定を持つ。生成されたアイテムはこの
	// GameObjectの子としてぶら下がる（RespawnItemsIfNoneExist参照）
	if (!FindObjectByTag(kGridItemSpawnerTag)) {
		GameObject& spawner = CreateObject("ItemSpawner");
		spawner.tag = kGridItemSpawnerTag;
		spawner.excludeFromPicking = true;
		spawner.AddComponent<GridItemSpawnComponent>();
	}

	// アイテムの初回配置（起動時に1つも存在しなければ配置する）はRespawnItemsIfNoneExist
	// （HandleSceneTransitionInputが毎フレーム呼ぶ）に委ねる。「全部拾ったらまた3つ出現する」
	// 挙動を実現するため、起動直後だけでなく毎フレーム判定する必要があるため、EnsureInitialObjectsExist
	// （needsInitialSpawn_の初回フレームでしか呼ばれない）とは別の関数にしてある

	// 壁スポナー（見た目・当たり判定を持たない空のGameObject）。GridWallSpawnComponentが
	// 「盤面に何枚の壁を存在させ続けるか」の設定を持つ。生成された壁はこのGameObjectの子として
	// ぶら下がる（RespawnWallsIfNoneExist参照）
	if (!FindObjectByTag(kGridWallSpawnerTag)) {
		GameObject& wallSpawner = CreateObject("WallSpawner");
		wallSpawner.tag = kGridWallSpawnerTag;
		wallSpawner.excludeFromPicking = true;
		wallSpawner.AddComponent<GridWallSpawnComponent>();
	}

	// 壁の初回配置はRespawnWallsIfNoneExist（HandleSceneTransitionInputが毎フレーム呼ぶ）に委ねる
	// （アイテムと同じ理由。壁は踏んでも消えないため通常は初回の1回しか出番が無いが、
	// 「リセット」ボタンで作り直した後の再配置にも同じ判定を使い回せるようにするため）

	// 行動可能マス数（残り移動コスト）を表示するテキスト（スクリーン空間UI）。実際の文字列は
	// 毎フレームUpdateCostTextが更新するため、ここでは箱だけ用意して仮の文字列でRebuildしておく。
	// Inspectorで位置・フォントサイズ・色を調整でき、その設定はscene.jsonに保存される
	// （textの中身自体は保存されても次フレームで必ず上書きされるため実害はない）
	if (!FindObjectByTag(kGridCostTextTag)) {
		GameObject& costText = CreateObject("CostText");
		costText.tag = kGridCostTextTag;
		costText.excludeFromPicking = true;
		costText.GetTransform().translation = { 20.0f, 20.0f, 0.0f };

		auto* text = costText.AddComponent<TextSpriteComponent>();
		text->fontSize = 28.0f;
		text->horizontalAlign = TextSpriteComponent::HorizontalAlign::kLeft;
		text->text = "行動可能マス: - / -";
		text->Rebuild(renderer_);

		costText.GetComponent<TransformComponent>()->is2D = true;
	}

	// 敵HPバー（盤面の上に置くワールド空間の3Dオブジェクト）
	EnsureEnemyHealthBarExists();

	// CreateObjectで追加したGameObjectをgizmoTargets_（Update/Draw対象一覧）に反映する
	RebuildDerivedLists();
}

void GridPuzzleScene::EnsureEnemyHealthBarExists() {
	// 赤/緑/青ごとに1本ずつ、合計kEnemyHealthBarCount本を独立したGameObjectとして持つ
	// （1種別1本の対応関係が崩れないよう、tagだけでなくGridItemComponent::Type単位で存在確認する）
	std::vector<GameObject*> existingBars;
	for (auto& obj : objects_) {
		if (obj->tag == kGridEnemyHealthBarTag) existingBars.push_back(obj.get());
	}

	// 旧バージョン（種別を問わず1本だけ、全種別監視）が残っている場合はいったん削除して
	// 3本構成へ作り直す
	bool isOldSingleBar = existingBars.size() == 1
		&& existingBars.front()->GetComponent<EnemyHealthBarComponent>()
		&& existingBars.front()->GetComponent<EnemyHealthBarComponent>()->IsWatching(GridItemComponent::Type::kAttackPower)
		&& existingBars.front()->GetComponent<EnemyHealthBarComponent>()->IsWatching(GridItemComponent::Type::kCostFixed)
		&& existingBars.front()->GetComponent<EnemyHealthBarComponent>()->IsWatching(GridItemComponent::Type::kCostRisky);
	if (isOldSingleBar) {
		DeleteObjects(existingBars);
		existingBars.clear();
	}

	if (static_cast<int>(existingBars.size()) < kEnemyHealthBarCount) {
		for (int i = 0; i < kEnemyHealthBarCount; ++i) {
			GameObject& healthBar = CreateObject("EnemyHealthBar");
			healthBar.tag = kGridEnemyHealthBarTag;
			healthBar.excludeFromPicking = true;
			healthBar.GetTransform().translation = EnemyHealthBarPosition(i);

			auto* bar = healthBar.AddComponent<EnemyHealthBarComponent>();
			bar->width = kEnemyHealthBarWidth;
			bar->height = kEnemyHealthBarHeight;
			bar->fillColor = kEnemyHealthBarSpecs[i].fillColor;
			bar->watchedTypes = { false, false, false };
			bar->watchedTypes[static_cast<size_t>(kEnemyHealthBarSpecs[i].watchedType)] = true;

			// アイテムは現在1種別につき1個（GridItemSpawnComponent::spawnEntriesの既定count=1）常に
			// 盤面に存在させ続ける方式のため、maxCollectCountも既定の「その種別のアイテム数」に合わせる
			bar->maxCollectCount = 3;

			// バーの上に「残量/最大値」ラベル（AlphabetTextComponent、例："2/3"）を子として追加する。
			// 実際の文字列はUpdateEnemyHealthBarLabelsが毎フレーム更新する（初期値はここでは仮置き）
			GameObject& label = CreateObject("HealthBarLabel");
			label.tag = kGridHealthBarLabelTag;
			label.excludeFromPicking = true;
			label.SetParent(&healthBar);
			label.GetTransform().translation = { 0.0f, 0.0f, kHealthBarLabelZOffset };

			auto* labelText = label.AddComponent<AlphabetTextComponent>();
			labelText->charScale = kHealthBarLabelCharScale;
			labelText->charSpacing = kHealthBarLabelCharSpacing;
			labelText->text = std::to_string(bar->maxCollectCount) + "/" + std::to_string(bar->maxCollectCount);
		}
	} else {
		// 既に3本とも存在する場合（以前のバージョンで作成済みのscene.jsonを読み込んだ場合等）でも、
		// ラベル子オブジェクトだけが無ければ後から追加する（既存のバー本体は作り直さない）
		for (GameObject* healthBar : existingBars) {
			bool hasLabel = false;
			for (GameObject* child : healthBar->GetChildren()) {
				if (child->tag == kGridHealthBarLabelTag) { hasLabel = true; break; }
			}
			if (hasLabel) continue;

			auto* bar = healthBar->GetComponent<EnemyHealthBarComponent>();
			if (!bar) continue;

			GameObject& label = CreateObject("HealthBarLabel");
			label.tag = kGridHealthBarLabelTag;
			label.excludeFromPicking = true;
			label.SetParent(healthBar);
			label.GetTransform().translation = { 0.0f, 0.0f, kHealthBarLabelZOffset };

			auto* labelText = label.AddComponent<AlphabetTextComponent>();
			labelText->charScale = kHealthBarLabelCharScale;
			labelText->charSpacing = kHealthBarLabelCharSpacing;
			labelText->text = std::to_string(bar->maxCollectCount) + "/" + std::to_string(bar->maxCollectCount);
		}
	}

	// ライフバー（4本目、赤/緑/青とは別枠）：1ターンで赤/緑/青を全部空にできなかった回数を表す
	if (!FindObjectByTag(kGridLifeHealthBarTag)) {
		GameObject& lifeBar = CreateObject("LifeHealthBar");
		lifeBar.tag = kGridLifeHealthBarTag;
		lifeBar.excludeFromPicking = true;
		lifeBar.GetTransform().translation = EnemyHealthBarPosition(kEnemyHealthBarCount);

		auto* bar = lifeBar.AddComponent<EnemyHealthBarComponent>();
		bar->width = kEnemyHealthBarWidth;
		bar->height = kEnemyHealthBarHeight;
		bar->fillColor = kLifeFillColor;
		bar->watchedTypes = { false, false, false }; // アイテム連動は使わない（Notify1FailureOccurred経由のみ）
		bar->maxCollectCount = kLifeMaxCount;
	}
}

void GridPuzzleScene::ResetAllEnemyHealthBars() {
	for (auto& obj : objects_) {
		if (obj->tag != kGridEnemyHealthBarTag) continue;
		if (auto* bar = obj->GetComponent<EnemyHealthBarComponent>()) bar->ResetCollectCount();
	}
}

void GridPuzzleScene::UpdateEnemyHealthBarLabels() {
	for (auto& obj : objects_) {
		if (obj->tag != kGridEnemyHealthBarTag) continue;
		auto* bar = obj->GetComponent<EnemyHealthBarComponent>();
		if (!bar) continue;

		for (GameObject* child : obj->GetChildren()) {
			if (child->tag != kGridHealthBarLabelTag) continue;
			if (auto* labelText = child->GetComponent<AlphabetTextComponent>()) {
				labelText->text = std::to_string(bar->GetRemainingCount()) + "/" + std::to_string(bar->maxCollectCount);
			}
		}
	}
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
			// 目地の隙間を作る）。Inspectorでマス間隔を変えるとタイルサイズもその場で追従する。
			// 盤面はX-Z平面（水平な地面）のため、薄くする軸はY（高さ方向）にする
			float tileSize = boardSize->cellSpacing * 0.9f;
			t.scale = { tileSize, kTileThickness, tileSize };

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

void GridPuzzleScene::UpdateCostText() {
	GameObject* textObj = FindObjectByTag(kGridCostTextTag);
	auto* text = textObj ? textObj->GetComponent<TextSpriteComponent>() : nullptr;
	if (!text) return;

	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	auto* playerMove = player ? player->GetComponent<GridBoardPlayerComponent>() : nullptr;
	if (!playerMove) return;

	std::string newText = "行動可能マス: " + std::to_string(playerMove->GetCurrentCost()) + " / " + std::to_string(playerMove->maxCost);

	// 値が変わった時だけRebuild（＝内部でCreateTextureFromPixelsを呼びテクスチャハンドルを新規消費する）
	// する。毎フレーム無条件に呼ぶとハンドルを際限なく浪費してしまうため
	if (newText != text->text) {
		text->text = newText;
		text->Rebuild(renderer_);
	}
}

void GridPuzzleScene::AdvanceTurnIfExecutionFinished() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	auto* playerMove = player ? player->GetComponent<GridBoardPlayerComponent>() : nullptr;
	bool isExecutingNow = playerMove && playerMove->GetPhase() == GridBoardPlayerComponent::Phase::kExecuting;

	// GridBoardPlayerComponentはReflexPlayerComponentと異なり「実行フェーズ完了→シーン側が
	// 明示的に計画フェーズへ戻す」準備フェーズを持たない（コンポーネント自身が自動遷移する）。
	// そのためシーン側は「実行フェーズだったか」を前フレームと比較するだけで、
	// kExecuting→kPlanningへ切り替わった瞬間（＝プレイヤーの行動マスが全て終わった瞬間）を検知する
	if (wasExecutingLastFrame_ && !isExecutingNow) {
		// リセット（ResetAllEnemyHealthBars）で赤/緑/青のバーが満タンに戻される前に、
		// このターン中に3本すべてを空にできたか（＝赤/緑/青のアイテムを1個ずつ全部取得できたか）を
		// 判定する。1本でも空にできていなければ「このターンは失敗」としてライフバーを1目盛り減らす
		bool allCleared = true;
		for (auto& obj : objects_) {
			if (obj->tag != kGridEnemyHealthBarTag) continue;
			auto* bar = obj->GetComponent<EnemyHealthBarComponent>();
			if (bar && !bar->IsFull()) { allCleared = false; break; }
		}
		if (!allCleared) {
			if (GameObject* lifeBarObj = FindObjectByTag(kGridLifeHealthBarTag)) {
				if (auto* lifeBar = lifeBarObj->GetComponent<EnemyHealthBarComponent>()) {
					lifeBar->Notify1FailureOccurred();
				}
			}
		}

		// RebuildItemsが取得済み・未取得を問わず全アイテムGameObjectを削除して作り直すため、
		// 削除済みポインタが残らないよう先にリストを空にしておく（Finalize相当の演出は行わず、
		// アイテムは壁と同じ「全削除→新規ランダム配置」でリセットする）。壁を先に作り直すことで、
		// アイテム側のスポーン抽選が新しい壁配置を避けられるようにする（逆順だとアイテムの上に
		// 壁が重なって配置され、指定した個数のアイテムが実質埋もれてしまう不具合があった）
		collectedItemsThisTurn_.clear();
		RebuildWalls();
		RebuildItems();

		// アイテムを全部作り直す＝取得数が0に戻るタイミングなので、敵HPバー（赤/緑/青）も
		// 満タンへ戻す（ライフバーはここでは触らない。失敗回数は貯まり続ける仕様のため）
		ResetAllEnemyHealthBars();
	}
	wasExecutingLastFrame_ = isExecutingNow;
}

std::vector<std::pair<int, int>> GridPuzzleScene::ComputeOccupiedCells(GridBoardComponent* boardSize) {
	std::vector<std::pair<int, int>> occupied;
	if (GameObject* player = FindObjectByTag(GameTags::kPlayer)) {
		int pc, pr;
		boardSize->WorldToNearestGrid(player->GetTransform().translation, pc, pr);
		occupied.push_back({ pc, pr });
	}
	for (auto& obj : objects_) {
		if (obj->tag != kGridItemTag) continue;
		auto* itemComp = obj->GetComponent<GridItemComponent>();
		if (itemComp && !itemComp->triggered) occupied.push_back({ itemComp->col, itemComp->row });
	}
	// 壁もアイテムと同じマスを取り合わないよう抽選母集団から除外する（アイテム側の抽選にも
	// 壁側の抽選にもこの関数を共通で使うため、双方が互いを避け合う形になる）
	for (auto& obj : objects_) {
		if (obj->tag != kGridWallTag) continue;
		auto* wallComp = obj->GetComponent<GridWallComponent>();
		if (wallComp) occupied.push_back({ wallComp->col, wallComp->row });
	}
	return occupied;
}

std::vector<std::pair<int, int>> GridPuzzleScene::ComputeFreeCells(GridBoardComponent* boardSize, const std::vector<std::pair<int, int>>& occupied) {
	std::vector<std::pair<int, int>> freeCells;
	for (int row = 0; row < boardSize->rows; ++row) {
		for (int col = 0; col < boardSize->columns; ++col) {
			bool isOccupied = false;
			for (const auto& cell : occupied) {
				if (cell.first == col && cell.second == row) { isOccupied = true; break; }
			}
			if (!isOccupied) freeCells.push_back({ col, row });
		}
	}
	return freeCells;
}

void GridPuzzleScene::UpdateCollectedItemsDisplay() {
	GameObject* spawner = FindObjectByTag(kGridItemSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridItemSpawnComponent>() : nullptr;
	if (!spawnConfig) return;

	// 敵HPバーは赤/緑/青の3本が独立して存在する（EnsureEnemyHealthBarExists参照）。取得された
	// アイテムのtypeを全バーへ通知し、実際に減るかどうかは各バーのwatchedTypes（Inspectorで
	// 選んだ監視対象種別）に委ねる
	std::vector<EnemyHealthBarComponent*> healthBars;
	for (auto& obj : objects_) {
		if (obj->tag != kGridEnemyHealthBarTag) continue;
		if (auto* bar = obj->GetComponent<EnemyHealthBarComponent>()) healthBars.push_back(bar);
	}

	// GridItemComponent::OnTriggerEnter（ColliderSystem::ResolveAndDraw経由）が効果適用済みの
	// アイテムにtriggered=trueを立てている。まだ取得済みリストに入っていないものだけを、
	// 見つけた順（＝取得した順）で追加する
	for (auto& obj : objects_) {
		if (obj->tag != kGridItemTag) continue;
		auto* itemComp = obj->GetComponent<GridItemComponent>();
		if (!itemComp || !itemComp->triggered) continue;

		bool alreadyListed = false;
		for (GameObject* listed : collectedItemsThisTurn_) {
			if (listed == obj.get()) { alreadyListed = true; break; }
		}
		if (!alreadyListed) {
			collectedItemsThisTurn_.push_back(obj.get());
			// 新たに取得されたアイテム1個につき、敵HPバーへ通知する
			for (auto* bar : healthBars) bar->NotifyItemCollected(itemComp->type);
		}
	}

	// 取得済みリストの全アイテムを、collectedDisplayTopを基準に積み重ねて配置する。
	// collectedDisplayLayoutがkVerticalならZ-方向（奥）へ、kHorizontalならX+方向（横）へ
	// 1個ずつずらす。col/rowは盤面へ戻すまで変更しない（SyncItemsもtriggered==trueの間は
	// ここで設定した位置を上書きしない）
	bool isHorizontal = spawnConfig->collectedDisplayLayout == GridItemSpawnComponent::DisplayLayout::kHorizontal;
	for (size_t i = 0; i < collectedItemsThisTurn_.size(); ++i) {
		GameObject* item = collectedItemsThisTurn_[i];
		if (!item) continue;
		Vector3 pos = spawnConfig->collectedDisplayTop;
		float offset = static_cast<float>(i) * spawnConfig->collectedDisplaySpacing;
		if (isHorizontal) pos.x += offset;
		else pos.z -= offset;
		item->GetTransform().translation = pos;
	}
}

void GridPuzzleScene::RespawnItemsIfNoneExist() {
	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!boardSize || boardSize->columns <= 0 || boardSize->rows <= 0) return;

	bool anyItemExists = false;
	for (auto& obj : objects_) {
		if (obj->tag == kGridItemTag) { anyItemExists = true; break; }
	}
	if (anyItemExists) return;

	// アイテムはスポナー（tag==kGridItemSpawnerTag、GridItemSpawnComponent::spawnEntriesが
	// 「種別ごとに何個出すか」を持つ）の子として生成する。無ければ何も生成しない
	// （EnsureInitialObjectsExistが必ず1つ用意するはずだが、念のためガードする）
	GameObject* spawner = FindObjectByTag(kGridItemSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridItemSpawnComponent>() : nullptr;
	if (!spawner || !spawnConfig) return;

	SpawnItemsFromConfig(*spawner, *spawnConfig, *boardSize);
}

void GridPuzzleScene::ResetItemsIfRequested() {
	GameObject* spawner = FindObjectByTag(kGridItemSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridItemSpawnComponent>() : nullptr;
	if (!spawner || !spawnConfig) return;
	if (!spawnConfig->ConsumeResetRequested()) return;

	RebuildItems();

	// 手動リセットでもアイテムが作り直される＝取得数が0に戻るため、敵HPバーも満タンへ戻す
	ResetAllEnemyHealthBars();
}

void GridPuzzleScene::RebuildItems() {
	GameObject* spawner = FindObjectByTag(kGridItemSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridItemSpawnComponent>() : nullptr;
	if (!spawner || !spawnConfig) return;

	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!boardSize || boardSize->columns <= 0 || boardSize->rows <= 0) return;

	// 現在盤面にあるアイテムを、取得済み（triggered==true）・未取得を問わず全部削除してから、
	// spawnEntries通りに新しく配置し直す（RespawnItemsIfNoneExistと異なり、既にアイテムが
	// 存在していても強制的に作り直す）
	std::vector<GameObject*> existingItems;
	for (auto& obj : objects_) {
		if (obj->tag == kGridItemTag) existingItems.push_back(obj.get());
	}
	if (!existingItems.empty()) DeleteObjects(existingItems);

	SpawnItemsFromConfig(*spawner, *spawnConfig, *boardSize);
}

void GridPuzzleScene::SpawnItemsFromConfig(GameObject& spawner, GridItemSpawnComponent& spawnConfig, GridBoardComponent& boardSize) {
	// 現時点で空いている全マスから、種別ごとの個数ぶんだけ重複なくランダムに抽選する
	std::vector<std::pair<int, int>> occupied = ComputeOccupiedCells(&boardSize);
	std::vector<std::pair<int, int>> freeCells = ComputeFreeCells(&boardSize, occupied);

	auto spawnItem = [&](GridItemComponent::Type type, int col, int row, const Vector4& color) {
		GameObject& item = CreateObject("Item");
		item.tag = kGridItemTag;
		item.SetParent(&spawner);
		item.GetTransform().scale = { kItemSize, kItemSize, kItemSize };
		Vector3 pos = boardSize.GridToWorld(col, row);
		item.GetTransform().translation = { pos.x, kItemHeightOffset, pos.z };

		auto* render = item.AddComponent<CubeRenderComponent>();
		render->color = color;
		render->lighting = false;

		auto* itemComp = item.AddComponent<GridItemComponent>();
		itemComp->type = type;
		itemComp->col = col;
		itemComp->row = row;
		itemComp->color = color; // Inspectorで調整する色の初期値（GridItemSpawnComponent::spawnEntriesの設定色）

		// プレイヤーとの当たり判定（アイテム取得）用。isTrigger=trueで押し戻しは行わない
		auto* itemCollider = item.AddComponent<OBBColliderComponent>();
		itemCollider->layer = CollisionLayer::kItem;
		itemCollider->isTrigger = true;
		itemCollider->halfSize = { kItemSize * 0.5f, kItemSize * 0.5f, kItemSize * 0.5f };
		};

	// 同じマンハッタン距離判定をクラスター中心の候補選び（事前チェック）と、本抽選の両方で使う
	auto manhattanDistance = [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
		return std::abs(a.first - b.first) + std::abs(a.second - b.second);
		};

	for (const auto& entry : spawnConfig.spawnEntries) {
		if (entry.count <= 0 || freeCells.empty()) continue;

		// クラスター中心を「候補ごとに半径内の空きマス数（自分を含む）」で評価し、count個
		// （盤面が狭くて誰も満たせない場合は、その時点での空きマス総数を上限とみなす）以上を
		// 満たせる候補だけからランダムに選ぶ。以前は完全ランダムに中心を選んでいたため、
		// 先に処理される種別（spawnEntries前方）がclusterRadius内の空きマスを広く占有してしまい、
		// 後続の種別は範囲内に空きが足りず範囲外へのフォールバックが頻発してバラバラに見える
		// 問題があった。「十分な余地がある場所」から中心を選ぶことでこれを避ける
		size_t neededCount = (std::min)(static_cast<size_t>(entry.count), freeCells.size());
		std::vector<size_t> viableCenters;
		size_t bestCapacity = 0;
		std::vector<size_t> bestCapacityCenters;
		for (size_t c = 0; c < freeCells.size(); ++c) {
			size_t capacity = 0;
			for (size_t j = 0; j < freeCells.size(); ++j) {
				if (manhattanDistance(freeCells[c], freeCells[j]) <= spawnConfig.clusterRadius) ++capacity;
			}
			if (capacity >= neededCount) viableCenters.push_back(c);
			if (capacity > bestCapacity) {
				bestCapacity = capacity;
				bestCapacityCenters.clear();
				bestCapacityCenters.push_back(c);
			} else if (capacity == bestCapacity) {
				bestCapacityCenters.push_back(c);
			}
		}
		// neededCountを満たす候補が1つも無ければ（盤面が狭い等）、最も空きマスを多く抱えている
		// 候補群から選ぶ（完全には収まらないが、可能な限り集める）
		const std::vector<size_t>& centerCandidates = !viableCenters.empty() ? viableCenters : bestCapacityCenters;

		std::uniform_int_distribution<size_t> centerDist(0, centerCandidates.size() - 1);
		size_t centerIndex = centerCandidates[centerDist(rng_)];
		std::pair<int, int> center = freeCells[centerIndex];
		freeCells.erase(freeCells.begin() + centerIndex);
		spawnItem(entry.type, center.first, center.second, entry.color);

		// 残り(count-1)個ぶんを、中心からclusterRadius以内にある空きマスから優先して抽選する
		// （見つからなければ範囲外のfreeCellsから補充し、盤面が狭くて範囲内に収まらない場合でも
		// 生成自体は続けられるようにする）
		for (int i = 1; i < entry.count; ++i) {
			if (freeCells.empty()) break; // 空きマスが尽きたらそれ以上は生成しない

			std::vector<size_t> nearbyIndices;
			for (size_t j = 0; j < freeCells.size(); ++j) {
				if (manhattanDistance(center, freeCells[j]) <= spawnConfig.clusterRadius) nearbyIndices.push_back(j);
			}

			size_t pickedIndex;
			if (!nearbyIndices.empty()) {
				std::uniform_int_distribution<size_t> nearbyDist(0, nearbyIndices.size() - 1);
				pickedIndex = nearbyIndices[nearbyDist(rng_)];
			} else {
				std::uniform_int_distribution<size_t> anyDist(0, freeCells.size() - 1);
				pickedIndex = anyDist(rng_);
			}

			std::pair<int, int> picked = freeCells[pickedIndex];
			freeCells.erase(freeCells.begin() + pickedIndex);

			spawnItem(entry.type, picked.first, picked.second, entry.color);
		}
	}

	// CreateObjectで追加したGameObjectをgizmoTargets_（Update/Draw対象一覧）に反映する
	RebuildDerivedLists();
}

void GridPuzzleScene::RespawnWallsIfNoneExist() {
	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!boardSize || boardSize->columns <= 0 || boardSize->rows <= 0) return;

	bool anyWallExists = false;
	for (auto& obj : objects_) {
		if (obj->tag == kGridWallTag) { anyWallExists = true; break; }
	}
	if (anyWallExists) return;

	GameObject* spawner = FindObjectByTag(kGridWallSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridWallSpawnComponent>() : nullptr;
	if (!spawner || !spawnConfig) return;

	SpawnWallsFromConfig(*spawner, *spawnConfig, *boardSize);
}

void GridPuzzleScene::ResetWallsIfRequested() {
	GameObject* spawner = FindObjectByTag(kGridWallSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridWallSpawnComponent>() : nullptr;
	if (!spawner || !spawnConfig) return;
	if (!spawnConfig->ConsumeResetRequested()) return;

	RebuildWalls();
}

void GridPuzzleScene::RebuildWalls() {
	GameObject* spawner = FindObjectByTag(kGridWallSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridWallSpawnComponent>() : nullptr;
	if (!spawner || !spawnConfig) return;

	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;
	if (!boardSize || boardSize->columns <= 0 || boardSize->rows <= 0) return;

	// 現在盤面にある壁を全部削除してから、pieceCount枚を新しく配置し直す
	// （RespawnWallsIfNoneExistと異なり、既に壁が存在していても強制的に作り直す）
	std::vector<GameObject*> existingWalls;
	for (auto& obj : objects_) {
		if (obj->tag == kGridWallTag) existingWalls.push_back(obj.get());
	}
	if (!existingWalls.empty()) DeleteObjects(existingWalls);

	SpawnWallsFromConfig(*spawner, *spawnConfig, *boardSize);
}

void GridPuzzleScene::ApplyImpassableToggleIfRequested() {
	GameObject* spawner = FindObjectByTag(kGridWallSpawnerTag);
	auto* spawnConfig = spawner ? spawner->GetComponent<GridWallSpawnComponent>() : nullptr;
	if (!spawner || !spawnConfig) return;
	if (!spawnConfig->ConsumeImpassableToggleRequested()) return;

	// 削除・再生成はせず、既存の壁GameObjectはそのままにcol/row・色を維持したまま
	// impassableフラグだけを一括で書き換える（「実行画面にある壁が瞬時に切り替わる」動作）
	for (auto& obj : objects_) {
		if (obj->tag != kGridWallTag) continue;
		if (auto* wallComp = obj->GetComponent<GridWallComponent>()) {
			wallComp->impassable = spawnConfig->impassable;
		}
	}
}

void GridPuzzleScene::SpawnWallsFromConfig(GameObject& spawner, GridWallSpawnComponent& spawnConfig, GridBoardComponent& boardSize) {
	auto spawnWallCell = [&](int col, int row) {
		GameObject& wall = CreateObject("Wall");
		wall.tag = kGridWallTag;
		wall.SetParent(&spawner);
		wall.GetTransform().scale = { kWallSize, kWallSize, kWallSize };
		Vector3 pos = boardSize.GridToWorld(col, row);
		wall.GetTransform().translation = { pos.x, kWallHeightOffset, pos.z };

		auto* render = wall.AddComponent<CubeRenderComponent>();
		render->color = spawnConfig.wallColor;
		render->lighting = false;

		auto* wallComp = wall.AddComponent<GridWallComponent>();
		wallComp->col = col;
		wallComp->row = row;
		wallComp->passCost = spawnConfig.passCost;
		wallComp->impassable = spawnConfig.impassable;
		wallComp->color = spawnConfig.wallColor; // Inspectorで調整する色の初期値
	};

	// 壁は手動デザイン基本形をそのまま使うのではなく、各基本形を90度回転4通り×左右反転2通り
	// （最大16種類、FixedWallPatternVariants参照）の中からランダムに1つ選んで配置する。
	// プレイヤー・アイテムが偶然そのマスに重なっていても、壁側は気にせずそのまま置く
	// （occupiedはプレイヤー・アイテム側のスポーン抽選が壁を避けるために使うものであり、
	// 固定配置の壁自身がそれらを避ける必要はない）
	const auto& variants = FixedWallPatternVariants();
	if (variants.empty()) return; // WallPatterns.jsonが読めなかった等の場合は壁を生成しない

	std::uniform_int_distribution<size_t> variantDist(0, variants.size() - 1);
	const auto& chosenPattern = variants[variantDist(rng_)];
	for (const auto& cell : chosenPattern) {
		spawnWallCell(cell.first, cell.second);
	}

	// CreateObjectで追加したGameObjectをgizmoTargets_（Update/Draw対象一覧）に反映する
	RebuildDerivedLists();
}

void GridPuzzleScene::SyncWalls() {
	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;

	for (auto& obj : objects_) {
		if (obj->tag != kGridWallTag) continue;
		auto* wallComp = obj->GetComponent<GridWallComponent>();
		if (!wallComp) continue;

		auto* render = obj->GetComponent<CubeRenderComponent>();
		if (render) render->color = wallComp->color;

		// col/row（配置マス座標）からワールド座標へ変換してTransformへ反映する。SyncItemsと同じ理由
		// （手動でInspectorからGridWallComponentをAdd Componentしてcol/rowを入力しただけでは、
		// GameObject自体のTransform.translationは変わらず盤面外に取り残されて見えなくなるため）
		if (boardSize) {
			Vector3 pos = boardSize->GridToWorld(wallComp->col, wallComp->row);
			obj->GetTransform().translation.x = pos.x;
			obj->GetTransform().translation.z = pos.z;
		}
	}
}

void GridPuzzleScene::SyncItems() {
	GameObject* board = FindObjectByTag(kGridBoardFolderTag);
	auto* boardSize = board ? board->GetComponent<GridBoardComponent>() : nullptr;

	for (auto& obj : objects_) {
		if (obj->tag != kGridItemTag) continue;
		auto* itemComp = obj->GetComponent<GridItemComponent>();
		if (!itemComp) continue;

		auto* render = obj->GetComponent<CubeRenderComponent>();
		if (render) render->color = itemComp->color;

		// 取得済み（triggered==true）のアイテムはUpdateCollectedItemsDisplayが取得済み表示位置へ
		// 直接Transformを書き換えているため、ここでは位置を触らない（触るとcol/row由来の
		// 古い盤面位置へ引き戻されてしまい、表示演出が機能しなくなる）
		if (itemComp->triggered) continue;

		// col/row（配置マス座標）からワールド座標へ変換してTransformへ反映する。手動でInspectorから
		// GridItemComponentをAdd Componentしてcol/rowを入力しただけでは、GameObject自体の
		// Transform.translationは変わらず盤面外（原点付近）に取り残されて見えなくなるため、
		// 毎フレームここで追従させる
		if (boardSize) {
			Vector3 pos = boardSize->GridToWorld(itemComp->col, itemComp->row);
			obj->GetTransform().translation.x = pos.x;
			obj->GetTransform().translation.z = pos.z;
		}
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
	// 表現する（reservedColor）。GetReservedPathCellsは予約地点（先端マス）だけでなく、
	// 実際に飛び越える途中のマスも含めた「これから進む全マス」を返す。行動可能マス
	// （validTargets）は予約マスと重なっていても常に行動可能マスの色を優先して塗る
	// （予約マスの色は「行動可能マスではない予約済みマス」にだけ使う）
	std::vector<std::pair<int, int>> validTargets = playerMove ? playerMove->GetValidTargets(player->GetTransform(), &gizmoTargets_) : std::vector<std::pair<int, int>>{};
	std::vector<std::pair<int, int>> reservedPathCells = playerMove ? playerMove->GetReservedPathCells(player->GetTransform(), &gizmoTargets_) : std::vector<std::pair<int, int>>{};
	Vector4 highlightColor = playerMove ? playerMove->highlightColor : boardSize->tileColorA;
	Vector4 reservedColor = playerMove ? playerMove->reservedColor : boardSize->tileColorA;

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
			bool isReserved = false;
			if (!isValidTarget) {
				for (const auto& cell : reservedPathCells) {
					if (cell.first == col && cell.second == row) { isReserved = true; break; }
				}
			}

			if (isValidTarget) render->color = highlightColor;
			else if (isReserved) render->color = reservedColor;
			else render->color = ((row + col) % 2 == 0) ? boardSize->tileColorA : boardSize->tileColorB;
		}
	}
}

REGISTER_SCENE(GridPuzzleScene, "GridPuzzle");
