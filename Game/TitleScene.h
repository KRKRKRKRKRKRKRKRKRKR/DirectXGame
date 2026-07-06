#pragma once
#include "IScene.h"

// タイトル画面。ENTERキーでSelectへ遷移する（内容は今後作り込む前提の最小スタブ）
class TitleScene : public IScene {
public:
	void Initialize(Renderer* renderer, Camera* camera) override;
	void Render(float deltaTime) override;
	SceneType GetNextScene() const override { return nextScene_; }

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	SceneType nextScene_ = SceneType::kNone;
};
