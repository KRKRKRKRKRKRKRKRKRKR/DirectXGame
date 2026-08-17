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

	// 準備フェーズ（PlayScene::UpdatePreparingPhase）で、直前の実行フェーズ中に倒した敵を
	// 1体補充スポーンしてから、次の1体を補充スポーンするまでの間隔（秒）。
	// 値を大きくするほど「1体ずつゆっくり湧く」演出になり、0にするとほぼ同時に全部湧く
	float respawnInterval = 0.3f;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
