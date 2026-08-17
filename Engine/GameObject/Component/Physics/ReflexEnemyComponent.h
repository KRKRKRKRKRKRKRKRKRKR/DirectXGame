#pragma once
#include "../../IComponent.h"
#include <string>

// 光反射アクションパズル「REFLEX」の敵（最小実装）。立方体の見た目で、自身は動かない
// （Updateは何もしない）。プレイヤーに接触したら消える。
// 既存のEnemyComponent（GraviTwist用、GravityFlipComponentへの参照回復処理を含む）とは
// 用途が異なるため流用せず新規に作る。「相手のColliderComponentBase::layerを見て自分が反応する」
// パターンと、「その場でGameObjectをeraseせずpendingDestroyを立てるだけの遅延破棄」パターンは
// EnemyComponent::OnTriggerEnterを踏襲する
//
// isTemplateがtrueのオブジェクトは「敵テンプレート」（旧ReflexEnemyTemplateComponent、
// 目印だけの別コンポーネントだったがここに統合した）。テンプレート自身は本物の敵として
// 動いてほしくないため、PlayScene::OnInitializeがisTemplate=trueのオブジェクトを見つけて
// enabled=falseにし・当たり判定を外し・フィールド外へ退避させる。PlayScene::SpawnEnemyAtは
// タグで見つけたテンプレートGameObjectからhp/hitShake等の値を読んで複製先へコピーする
class ReflexEnemyComponent : public IComponent {
public:
	bool enabled = true;

	// trueなら「敵テンプレート」。実際に動く敵ではなく、PlayScene::SpawnEnemyAtが複製元として
	// 参照するだけのひな形（Play開始時にPlayScene::OnInitializeが自動的に隠す）
	bool isTemplate = false;

	// この敵がPlayScene::SpawnEnemyAtで複製された際の複製元テンプレートタグ（例："AEnemy"）。
	// 撃破された瞬間、PlayScene::ProcessPendingDestroysがこの値をpendingRespawnTags_へ記録する。
	// 実行フェーズが完了して準備フェーズに入ると、PlayScene::BeginPreparingPhase/UpdatePreparingPhaseが
	// これを基に「倒した種類を1体ずつ間隔を空けて」補充スポーンする。
	// 保存対象：この値が保存されないと、シーン保存→再読み込みで復元された敵（前回プレイの
	// 途中状態がそのままscene.jsonに残っているケース）はspawnedFromTagが空になり、倒しても
	// 補充スポーンされなくなる（テンプレート自身はspawnedFromTagを設定しないため常に空のまま）
	std::string spawnedFromTag;

	// ColliderSystem::ResolveAndDrawのループ中からOnTriggerEnterが呼ばれるため、その場で
	// GameObjectをunique_ptrごとeraseするとダングリングになる。フラグだけ立てておき、
	// ループ完了後（シーン側のHandleSceneTransitionInput冒頭等）にまとめてDeleteObjectsで回収する
	bool pendingDestroy = false;

	// 直撃した（ダメージが入った）瞬間にtrueになる。OnTriggerEnterはオーナーGameObjectへの
	// 参照を持たないため、座標を伴うパーティクル生成はここではなくPlayScene::ProcessPendingDestroys側で
	// 「このフラグが立っているオブジェクト自身のGetWorldTransform()」を見て行う。撃破と同じフレームでも
	// 独立してtrueになる（"生きている間の被弾"と"撃破"を別演出として両方出したいケースのため）
	bool pendingHitParticle = false;

	// 直撃した瞬間にtrueになる。pendingHitParticleと同じ理由（OnTriggerEnterが自分のGameObjectへの
	// 参照を持たない）で、実際にHitSoundComponent::Play()を呼ぶのはPlayScene::ProcessPendingDestroys側。
	// 同じGameObjectにHitSoundComponentが付いていない場合は何も鳴らない（無くても壊れない）
	bool pendingHitSound = false;

	// HP。プレイヤーに接触するたびReflexPlayerAttackComponent::attackPower分だけ減算し、
	// 0以下になった瞬間にpendingDestroyを立てる（1発で倒したい場合はhpを低く設定すればよい）
	float maxHp = 10.0f;
	float hp = 10.0f;

	// プレイヤーに接触した瞬間にHitEffect::RequestShake/RequestHitStopへ渡す強さ・継続時間。
	// 「どれだけ揺らす/止めるか」は攻撃側（この敵）の性質なので、カメラ側ではなくここで持つ
	// （タンク種は強め、雑魚は弱め、等の差を将来つけやすくする狙い）
	float hitShakeStrength = 0.25f;
	float hitShakeDuration = 0.15f;
	float hitStopDuration = 0.05f;

	void OnTriggerEnter(GameObject& other) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
