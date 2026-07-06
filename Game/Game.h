#pragma once
#include "../Engine/Graphics/Renderer/Renderer.h"
#include "../Engine/Camera/Camera.h"
#include "SceneManager.h"

// アプリの器。Renderer/Cameraの橋渡し、FPS計測、"FPS"/"Camera"パネルの描画だけを担当する。
// Title/Select/Play/GameOverの切り替えはSceneManagerが担い、各シーンの中身
// （GameObject群・当たり判定・Gizmo編集・ライティング等）はそれぞれのIScene実装側にある
class Game {
public:
	Game() = default;
	~Game() = default;

	void Initialize(Renderer* renderer, Camera* camera);
	void Update(float deltaTime);
	void Render();

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	float deltaTime_ = 0.0f;

	SceneManager sceneManager_;

	// FPS表示（負荷テスト用）。0.5秒ごとに直近フレームの平均FPS/フレーム時間を更新する
	// （毎フレームの値は変動が激しく読みづらいため、一定間隔でサンプリングして表示する）
	float fpsSampleTimer_    = 0.0f;
	int   fpsSampleFrames_   = 0;
	float fpsDisplayValue_   = 0.0f;
	float frameTimeDisplayMs_ = 0.0f;

	void DrawImGui();
};
