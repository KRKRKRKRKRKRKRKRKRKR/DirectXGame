#pragma once
#include "SceneBase.h"

// ゲームプレイ画面。GameObjectエディタ機能一式はSceneBaseが提供し、本クラスは
// 「初期HUDとしてCamera Coordを1つ置く」「ESCでTitle・F1でGameOverへ遷移する」という
// PlayScene固有の差分だけを持つ
class PlayScene : public SceneBase {
protected:
	void OnInitialize() override;
	void HandleSceneTransitionInput() override;

	// EnemyComponent::pendingDestroy==trueのオブジェクトをまとめて回収する。
	// ColliderSystem::ResolveAndDrawのループ中（OnTriggerEnterの中）ではGameObjectを
	// その場でeraseできないため、ループが完全に終わった後のこのタイミングでまとめて処理する
	void ProcessPendingDestroys();
};
