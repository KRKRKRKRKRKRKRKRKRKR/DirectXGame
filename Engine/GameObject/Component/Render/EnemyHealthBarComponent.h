// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../Physics/GridItemComponent.h"
#include <algorithm>
#include <array>

// パイプ接続パズル企画の「敵」表現として、盤面上部にワールド空間の3Dオブジェクト（Cube2枚：
// 背景＋塗りつぶし）で表示する敵HPバー（今回のスコープはバーのみ：敵自身の見た目・行動
// ロジックは後回し）。プレイヤーがアイテムを取得した回数に応じてバーが減っていき、
// maxCollectCount回取得した時点で0になる（割合=取得数/maxCollectCount）。
// ReflexEnemyHealthBarComponentと同じ「Update内で直接Renderer::DrawCubeを呼ぶ」方式
// （texture=kTextureNoneで単色のCubeを描く）。ReflexEnemyHealthBarComponentが敵の頭上に
// 追従するのに対し、こちらはオーナーGameObjectのtransform.translationをそのままバー中心
// として使う固定配置（GridPuzzleSceneが盤面の外・上に置く想定）。
//
// どのアイテム種別の取得をこのバーの減少条件にするかをInspectorから選べる（watchedTypes、
// GridItemComponent::Type単位のチェックボックス。GridPuzzleScene::UpdateCollectedItemsDisplayが
// アイテムを1個取得するたびにNotifyItemCollected(type)を呼び、このコンポーネントは
// watchedTypesでtrue（監視対象）になっている種別の取得だけをカウントする。個々のアイテム
// GameObjectは毎ターンRebuildItemsで全削除・再生成されて参照が不安定なため、
// 「種別」という安定した単位で選ばせる）
class EnemyHealthBarComponent : public IComponent {
public:
	// この回数ぶんアイテムを取得された時点でバーが空になる
	int maxCollectCount = 10;

	// バーの見た目サイズ（ワールド単位）
	float width = 3.0f;
	float height = 0.4f;

	// バー未使用部分（背景）と残量部分（塗りつぶし）の色
	Vector4 backgroundColor = { 0.15f, 0.15f, 0.18f, 0.9f };
	Vector4 fillColor = { 0.85f, 0.2f, 0.25f, 1.0f };

	// 表示中の割合(displayRatio_)が目標値へ近づく速さ（1秒あたりの変化量、0〜1換算）。
	// 大きいほど瞬時に近くなる
	float shrinkSpeed = 2.0f;

	// このバーの減少条件として監視するアイテム種別。true になっている種別の取得数合計が
	// currentCollectCount_（maxCollectCountに対する割合の分子）になる。既定は3種類とも監視する
	// （旧仕様＝アイテムを何でも1個取得するたびに減る、と同じ挙動）
	std::array<bool, 3> watchedTypes = { true, true, true };

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	bool IsWatching(GridItemComponent::Type type) const {
		return watchedTypes[static_cast<size_t>(type)];
	}

	// 指定種別のアイテムを1個取得したことをこのバーへ通知する。GridPuzzleScene側がアイテム
	// 取得を検知したタイミング（GridItemComponent::triggeredがfalse→trueに変わった瞬間）ごとに、
	// そのアイテムのtypeを添えて呼ぶ想定。watchedTypesで監視対象になっていない種別は無視する
	void NotifyItemCollected(GridItemComponent::Type type) {
		if (!IsWatching(type)) return;
		if (currentCollectCount_ < maxCollectCount) ++currentCollectCount_;
	}

	// 種別を問わず無条件に1回分カウントする。ライフバー（GridPuzzleScene::kGridLifeHealthBarTag、
	// 「1ターンで赤/緑/青を全部空にできなかった回数」を表す4本目のバー）用。watchedTypesは
	// 参照しない（そもそもアイテム取得ではなく「ターン失敗」1回を1目盛りとして数えるため）
	void Notify1FailureOccurred() {
		if (currentCollectCount_ < maxCollectCount) ++currentCollectCount_;
	}

	// 現在の取得数が既にmaxCollectCountへ到達している（＝このバーが空になっている）かどうか。
	// GridPuzzleScene::AdvanceTurnIfExecutionFinishedが「赤/緑/青の3本を1ターンで
	// 全部空にできたか」を判定するために使う
	bool IsFull() const { return currentCollectCount_ >= maxCollectCount; }

	// バーの残量（maxCollectCount - currentCollectCount_、0未満にはならない）。
	// GridPuzzleScene::UpdateEnemyHealthBarLabelsが「残量/最大値」の文字列表示に使う
	// （バー自体は減っていく方向の見た目のため、表示もそれに合わせて「残量」を分子にする）
	int GetRemainingCount() const { return (std::max)(0, maxCollectCount - currentCollectCount_); }

	// 現在の取得数を明示的にリセットする（盤面リセット等でバーも満タンへ戻したい場合に使う）
	void ResetCollectCount() { currentCollectCount_ = 0; }

private:
	// これまでに取得された回数（実行時の一時状態、非保存）。0〜maxCollectCount
	int currentCollectCount_ = 0;

	// 実際に描画される残量割合（0〜1）。currentCollectCountから求まる目標値へ、Updateで
	// shrinkSpeedに応じてなめらかに近づける（実行時の一時状態、非保存）
	float displayRatio_ = 1.0f;
};
