#pragma once

// Sceneの種類。SceneManagerがこの値を見て生成する具体的なIScene派生クラスを切り替える
enum class SceneType {
	kNone, // 遷移しない（IScene::GetNextScene()のデフォルト戻り値。SceneManagerがこれを渡すことは想定しない）
	kTitle,
	kSelect,
	kPlay,
	kGameOver,
};
