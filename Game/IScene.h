#pragma once
#include "SceneType.h"

class Renderer;
class Camera;

// Title/Select/Play/GameOver等、画面（シーン）の共通インターフェース。
// GameObject/コンポーネント階層は一貫して非virtual（GetComponent<具体型>()で取得した
// 具体的な型に直接メソッドを呼ぶ）だが、SceneManagerは次にどのシーンが来るか実行時まで
// 分からないため、ここは素直にvirtualで動的ディスパッチする
class IScene {
public:
	virtual ~IScene() = default;

	virtual void Initialize(Renderer* renderer, Camera* camera) = 0;
	virtual void Render(float deltaTime) = 0;

	// 遷移したい場合は次のSceneTypeを返す。デフォルトは「遷移しない」
	virtual SceneType GetNextScene() const { return SceneType::kNone; }
};
