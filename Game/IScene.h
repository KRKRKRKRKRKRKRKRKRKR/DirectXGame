#pragma once
#include "SceneType.h"
#include <string>

class Renderer;
class Camera;

// Title/Select/Play/GameOver等、画面（シーン）の共通インターフェース。
// GameObject/コンポーネント階層は一貫して非virtual（GetComponent<具体型>()で取得した
// 具体的な型に直接メソッドを呼ぶ）だが、SceneManagerは次にどのシーンが来るか実行時まで
// 分からないため、ここは素直にvirtualで動的ディスパッチする
class IScene {
public:
	virtual ~IScene() = default;

	// assetFolderはSceneManagerがSceneTypeから決めるシーン専用の素材フォルダ名（例: "Play"）。
	// PlayScene等はResources/{assetFolder}/にUI/Object用のJSONを保存・復元することで、
	// シーンごとに別々の素材セットを切り替えられるようにする
	virtual void Initialize(Renderer* renderer, Camera* camera, const std::string& assetFolder) = 0;
	virtual void Render(float deltaTime) = 0;

	// 遷移したい場合は次のSceneTypeを返す。デフォルトは「遷移しない」
	virtual SceneType GetNextScene() const { return SceneType::kNone; }

	// アプリ終了確認（Window::IsCloseRequested）等、シーン非依存のコードから「今のシーンを
	// 保存してほしい」と伝えるためのフック。デフォルトは何もしない（SceneBaseがオーバーライドして
	// 既定のSaveScene()を呼ぶ）
	virtual void RequestSave() {}
};
