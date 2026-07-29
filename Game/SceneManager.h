#pragma once
#include "IScene.h"
#include <memory>

class Renderer;
class Camera;

// 現在のSceneを1つだけ保持し、IScene::GetNextScene()の戻り値を見て毎フレーム切り替えを行う。
// 複数シーンの同時ロードや先読み等は行わない、常に1つを生成・破棄する単純な方式にしている
class SceneManager {
public:
	void Initialize(Renderer* renderer, Camera* camera, SceneType startScene);
	void Render(float deltaTime);

	// アプリ終了確認等、現在のシーンに対して外部から操作したい呼び出し元向け
	// （Game::Updateがアプリ終了時の保存確認でRequestSave()を呼ぶ）
	IScene* GetCurrentScene() const { return currentScene_.get(); }

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	std::unique_ptr<IScene> currentScene_;

	void ChangeScene(SceneType next);
	std::unique_ptr<IScene> CreateScene(SceneType type);

	// SceneTypeごとの素材フォルダ名（Resources/{フォルダ名}/）。シーン間で名前が衝突しないよう
	// SceneType名とそのまま対応させている
	static std::string GetAssetFolderName(SceneType type);
};
