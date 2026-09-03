// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "GridItemComponent.h"
#include <string>
#include <vector>

// パイプ接続パズル企画のアイテムスポーン設定を、Inspectorから調整できるようにするための
// コンポーネント。ReflexEnemySpawnerComponentと同じ方針：「どの種類を何個、盤面に常に
// 存在させ続けるか」という設定値の置き場所のみを持ち、実際の生成・再配置ロジック
// （空きマスの抽選・GameObjectの生成・親子付け）はGridPuzzleScene側に置いたままにする
// （「シーン全体を走査してGameObjectを生成する」処理は1つのGameObjectに属さないため、
// IComponentよりシーンクラス側の責務として扱う既存方針を踏襲する）。
//
// 生成されたアイテム（GridItemComponent付きGameObject）は、このコンポーネントが付いた
// GameObject（スポーナー）の子オブジェクトとしてぶら下がる（GridPuzzleScene::
// RespawnItemsIfNoneExistがSetParentする）。ヒエラルキー上でアイテム一式をまとめて見渡せる
// ようにするため（PlayScene::GetOrCreateGroupFolderと同じ「フラットに埋めない」工夫）。
class GridItemSpawnComponent : public IComponent {
public:
	// アイテム種類1件分：どの種別を、盤面上に同時に何個存在させ続けるか、生成時にどの色を
	// 与えるか
	struct SpawnEntry {
		GridItemComponent::Type type = GridItemComponent::Type::kAttackPower;
		int count = 1;
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	};

	// アイテム種類の一覧。GridPuzzleScene::RespawnItemsIfNoneExist/SpawnItemsFromConfigが
	// （盤面上にtag==kGridItemTagが1つも無い時、またはリセットボタンが押された時）この一覧を読み、
	// 各エントリのcolorを与えつつcount個ぶんランダムな空きマスへ生成する。1体拾われて再配置される際
	// （GridPuzzleScene::FinalizeCollectedItemsOnTurnEnd）は色・個数を変えず、同じ種別のまま
	// 別の空きマスへ移すだけなので、このエントリ一覧を読み直す必要はない。生成済みの子アイテム
	// 個々の色（GridItemComponent::color）は生成後Inspectorから個別に上書きできる
	// （ここでの色はあくまで新規生成時の初期値）
	std::vector<SpawnEntry> spawnEntries = {
		SpawnEntry{ GridItemComponent::Type::kAttackPower, 1, { 0.9f, 0.2f, 0.2f, 1.0f } },  // 赤
		SpawnEntry{ GridItemComponent::Type::kCostFixed, 1, { 0.2f, 0.5f, 0.95f, 1.0f } },   // 青
		SpawnEntry{ GridItemComponent::Type::kCostRisky, 1, { 0.85f, 0.55f, 0.95f, 1.0f } }, // 紫
	};

	// プレイヤーが取得した（GridItemComponent::triggered==trueになった）アイテムを、盤面外の
	// 表示領域へ一時的に並べる際の一番上の座標（ワールド座標）。GridPuzzleScene::
	// UpdateCollectedItemsDisplayが取得した順に、この座標から奥（Z-方向）へcollectedDisplaySpacing
	// 間隔で積み重ねて配置する。盤面がX-Z平面（水平な地面、Y=0）上に広がるため、Y成分は
	// 地面より少し高い位置（プレイヤー・アイテムと同じ考え方）にしておく。既定値は盤面の
	// 左側（グリッドタイルの外）を想定した値
	Vector3 collectedDisplayTop = { -2.5f, 0.3f, 0.0f };

	// 取得済みアイテムを積み重ねる間隔（ワールド単位、Z-方向）
	float collectedDisplaySpacing = 1.0f;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// DrawImGuiの「リセット」ボタンが押された瞬間だけtrueを返し、呼ぶとフラグを消費する
	// （ReflexPlayerComponent::ConsumeExecutionFinishedと同じワンショットの取り出し方）。
	// GridPuzzleScene::ResetItemsIfRequestedが毎フレームこれを見て、trueなら現在盤面にある
	// アイテムを全部削除してからspawnEntries通りに新しく配置し直す。実行時の一時要求のため
	// ToJson/FromJsonでは保存しない
	bool ConsumeResetRequested() {
		bool result = resetRequested_;
		resetRequested_ = false;
		return result;
	}

private:
	bool resetRequested_ = false;
};
