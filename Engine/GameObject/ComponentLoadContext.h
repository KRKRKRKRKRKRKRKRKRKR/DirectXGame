#pragma once
#include <vector>

class Renderer;
struct TextureEntry;

// JSONからコンポーネントを復元する際、コンストラクタ引数として外部リソースが要る
// コンポーネント（ModelRenderComponent、TextureSelectorComponent等）に渡す共有コンテキスト。
// 将来別の外部依存が増えてもここにフィールドを足すだけで済むようにまとめている
struct ComponentLoadContext {
	Renderer* renderer = nullptr;
	const std::vector<TextureEntry>* textures = nullptr;
};
