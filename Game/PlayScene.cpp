#include "PlayScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void PlayScene::OnInitialize() {
	// Camera座標を毎フレーム表示するHUD。表示内容はTextProviderとして1回登録するだけで、
	// 以降はSceneBase::Render内の汎用ループが毎フレーム自動的にUpdateDynamicText()を呼んでくれる
	CreateHud("Camera Coord");
}

void PlayScene::HandleSceneTransitionInput() {
	// ColliderSystem::ResolveAndDrawの直後（Renderの最後）に呼ばれるこのタイミングで、
	// 今フレーム体当たりされた敵をまとめて回収する
	ProcessPendingDestroys();

	// デバッグ用キー割り当て（ESCでTitle、F1でGameOverへ遷移）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = SceneType::kTitle;
	if (Input::IsTriggered(DIK_F1))     nextScene_ = SceneType::kGameOver;

	// タグ"Player"のオブジェクトを毎フレーム見に行き、HealthComponentのHPが0になっていたら
	// GameOverへ遷移する（ポーリング方式。AutoRun/CameraFollowの対象探しと同じやり方に揃えている）
	if (GameObject* player = FindObjectByTag("Player")) {
		if (auto* hp = player->GetComponent<HealthComponent>()) {
			if (hp->IsDead()) nextScene_ = SceneType::kGameOver;
		}
	}
}

void PlayScene::ProcessPendingDestroys() {
	std::vector<GameObject*> toDestroy;
	for (auto& obj : objects_) {
		if (auto* enemy = obj->GetComponent<EnemyComponent>()) {
			if (enemy->pendingDestroy) toDestroy.push_back(obj.get());
		}
	}
	if (!toDestroy.empty()) DeleteObjects(toDestroy);
}
