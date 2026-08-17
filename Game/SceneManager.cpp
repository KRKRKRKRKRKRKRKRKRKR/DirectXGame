#include "SceneManager.h"
#include "SceneRegistry.h"
#include "../Engine/Utils/Logger.h"

void SceneManager::Initialize(Renderer* renderer, Camera* camera, const std::string& startScene) {
	renderer_ = renderer;
	camera_ = camera;
	ChangeScene(startScene);
}

void SceneManager::Render(float deltaTime) {
	currentScene_->Render(deltaTime);

	// Render完了後に遷移要求を確認する（描画中にシーンが差し替わらないようにするため）
	std::string next = currentScene_->GetNextScene();
	if (!next.empty()) {
		ChangeScene(next);
	}
}

void SceneManager::ChangeScene(const std::string& next) {
	auto scene = SceneRegistry::Create(next);
	if (!scene) {
		Logger::Log("SceneManager: 未登録のシーン名が指定されました: " + next);
		return;
	}
	currentScene_ = std::move(scene);
	// アセットフォルダ名はシーン名をそのまま使う（Resources/{シーン名}/）
	currentScene_->Initialize(renderer_, camera_, next);
}
