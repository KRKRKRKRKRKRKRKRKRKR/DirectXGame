#include "PlayScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void PlayScene::OnInitialize() {
	// Camera座標を毎フレーム表示するHUD。表示内容はTextProviderとして1回登録するだけで、
	// 以降はSceneBase::Render内の汎用ループが毎フレーム自動的にUpdateDynamicText()を呼んでくれる
	CreateHud("Camera Coord");
}

void PlayScene::HandleSceneTransitionInput() {
	// デバッグ用キー割り当て（ESCでTitle、F1でGameOverへ遷移）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = SceneType::kTitle;
	if (Input::IsTriggered(DIK_F1))     nextScene_ = SceneType::kGameOver;
}
