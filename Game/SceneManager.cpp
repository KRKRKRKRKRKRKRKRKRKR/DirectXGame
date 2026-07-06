#include "SceneManager.h"
#include "TitleScene.h"
#include "SelectScene.h"
#include "PlayScene.h"
#include "GameOverScene.h"

void SceneManager::Initialize(Renderer* renderer, Camera* camera, SceneType startScene) {
	renderer_ = renderer;
	camera_ = camera;
	ChangeScene(startScene);
}

void SceneManager::Render(float deltaTime) {
	currentScene_->Render(deltaTime);

	// Render完了後に遷移要求を確認する（描画中にシーンが差し替わらないようにするため）
	SceneType next = currentScene_->GetNextScene();
	if (next != SceneType::kNone) {
		ChangeScene(next);
	}
}

void SceneManager::ChangeScene(SceneType next) {
	currentScene_ = CreateScene(next);
	currentScene_->Initialize(renderer_, camera_);
}

std::unique_ptr<IScene> SceneManager::CreateScene(SceneType type) {
	switch (type) {
		case SceneType::kTitle:    return std::make_unique<TitleScene>();
		case SceneType::kSelect:   return std::make_unique<SelectScene>();
		case SceneType::kPlay:     return std::make_unique<PlayScene>();
		case SceneType::kGameOver: return std::make_unique<GameOverScene>();
		default:                   return nullptr; // kNoneでここに来ることは想定しない
	}
}
