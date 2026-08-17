#pragma once
#include <vector>
#include <string>
#include <functional>

class Renderer;
struct TextureEntry;
struct ProjectAssetEntry;

// JSONからコンポーネントを復元する際、コンストラクタ引数として外部リソースが要る
// コンポーネント（ModelRenderComponent、TextureSelectorComponent等）に渡す共有コンテキスト。
// 将来別の外部依存が増えてもここにフィールドを足すだけで済むようにまとめている
struct ComponentLoadContext {
	Renderer* renderer = nullptr;
	const std::vector<TextureEntry>* textures = nullptr;

	// pathの画像がtextures内に未登録なら登録する（実体はSceneBase::EnsureTextureRegistered）。
	// ModelRenderComponentのサブメッシュ別テクスチャコンボへ、プロジェクトパネルからまだ
	// 登録されていない画像がドラッグ&ドロップされた場合に使う。texturesと同じ実体を指す
	// vectorへpush_backするだけなので、呼んだ直後にtexturesを再検索すれば見つかる
	std::function<void(const std::string&)> ensureTextureRegistered;

	// SceneBase::projectAudioClips_（Resources/配下を走査して見つかった音声ファイル一覧）への
	// 非所有ポインタ。HitSoundComponentがTextureSelectorComponentと同じ「コンボで選ぶ」方式の
	// SE選択に使う
	const std::vector<ProjectAssetEntry>* audioClips = nullptr;
};
