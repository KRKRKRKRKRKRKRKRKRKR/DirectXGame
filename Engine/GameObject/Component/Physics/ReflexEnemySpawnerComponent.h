#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"
#include <vector>
#include <string>

// 光反射アクションパズル「REFLEX」の敵リスポーン設定を、Inspectorから調整できるようにする
// ためのコンポーネント。実際のリスポーン処理（PlayScene::RespawnEnemiesIfCleared等）は
// このコンポーネントの値を読むだけで、生成ロジック自体はPlayScene側に置いたままにする
// （「シーン全体を走査してGameObjectを生成する」処理は1つのGameObjectに属さないため、
// IComponentよりシーンクラス側の責務として扱う方針は変えない。ここは設定値の置き場所のみ）
//
// 敵の種類は「シーン上に実際に配置したテンプレートGameObject」で表す。例えばtag="A"の
// GameObjectに見た目（CubeRenderComponent）とReflexEnemyComponentのパラメータを設定しておき、
// このコンポーネントにはそのタグ名と出現数だけを登録する。実際のスポーン処理
// （PlayScene::SpawnEnemyAt）がタグ名からテンプレートを検索し、見た目・パラメータを複製する。
// テンプレート自体はPlayScene::OnInitializeで当たり判定を外して隠される（本物の敵と重複させない）
class ReflexEnemySpawnerComponent : public IComponent {
public:
	// 敵の種類1件分：どのタグのテンプレートを、何体出すか
	struct SpawnEntry {
		std::string tag = "A";
		int         count = 4;
	};

	// 敵種類の一覧。全滅判定時、ここに登録された全エントリの数だけ、対応タグのテンプレートを
	// 複製して生成する（例：tag="A" count=4, tag="B" count=2 なら計6体）
	std::vector<SpawnEntry> spawnEntries = { SpawnEntry{} };

	// 準備フェーズ（PlayScene::UpdatePreparingPhase）で敵を1体スポーンしてから、次の1体を
	// スポーンするまでの間隔（秒）。この[spawnIntervalMin, spawnIntervalMax]の範囲から
	// 1体出すたびにランダムに抽選する（min==maxにすれば固定間隔になる、旧仕様と同じ挙動）。
	// このキューはPlayシーンを開いた直後の初回スポーンと、実行フェーズ完了後の補充スポーンの
	// 両方で使われる
	float spawnIntervalMin = 0.3f;
	float spawnIntervalMax = 0.3f;

	// スポーンするたびに、テンプレートの見た目サイズ（Transform.scale）・当たり判定サイズに
	// 掛けるランダム倍率の範囲。1.0が等倍（テンプレート通りのサイズ）。min==maxにすればランダムに
	// せず常に同じ倍率になる。PlayScene::SpawnEnemyAtが毎回この範囲から1つ抽選して使う
	float sizeScaleMin = 0.8f;
	float sizeScaleMax = 1.2f;

	// 敵をスポーンする位置とプレイヤーとの間に必ず空けておく最小距離。PlayScene::
	// BuildShuffledSpawnGrid/PickEnemySpawnPositionがこの半径内のマス・座標をスポーン候補から
	// 除外する。プレイヤーが移動していないのに敵の出現と重なって接触ダメージを受けてしまう
	// 場合はこの値を広げる
	float playerExclusionRadius = 3.0f;

	// 敵をスポーンできるワールド座標の範囲（X/Yそれぞれ独立に指定）。以前はX/Y共通の正方形
	// （spawnRangeMin/Max 1組）だったが、壁が正方形とは限らない（例：横長のフィールド）ため、
	// ReflexPlayerComponent::fieldRangeMinX/MaxX/MinY/MaxYと同じ「X/Y独立の4値」方式に変更した。
	// 壁の内側（壁の厚み・敵自身の半径ぶんの余裕）に収まるよう設定すること
	float spawnRangeMinX = -8.0f;
	float spawnRangeMaxX = 8.0f;
	float spawnRangeMinY = -8.0f;
	float spawnRangeMaxY = 8.0f;

	// 敵同士の最小離隔距離（フォールバック抽選時のみ使用。通常時はグリッド配置自体が
	// 重なりを防ぐ）。以前はPlayScene.cpp内のkMinSpawnDistance定数で固定されていた
	float minSpawnDistance = 2.0f;

	// スポーン用グリッドのセル間隔。敵の当たり判定サイズより十分広く取り、隣接セルに配置された
	// 敵同士が接触しないようにする。以前はPlayScene.cpp内のkSpawnGridCellSize定数で固定されていた
	float spawnGridCellSize = 1.5f;

	// クリアまでの目標ラウンド数（PlayScene::roundCount_がkPreparing→kPlanningの遷移でこの値に
	// 達したらClearSceneへ遷移する）。ゲームロジック自体はPlayScene側が持つが、値そのものは
	// GameObjectのコンポーネントデータとしてここに置くことで、シーンJSON保存/読込の既存の仕組み
	// （SceneObjectStore::Save/Load）にそのまま乗せられるようにする（PlayScene自身のメンバに
	// 直接持たせると、シーンをまたいで永続化する手段がなくInspectorで変更しても保存されなかった）
	int totalRounds = 30;

	// Sceneビュー（エディタ自由カメラ）表示中のみ、spawnRangeMin/Maxの範囲を
	// ReflexPlayerComponentの移動可能範囲ワイヤーフレームと同じ見た目で可視化する。
	// Gameビュー中は描画しない（デバッグ用の補助線を実プレイ画面に映り込ませないため）
	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
