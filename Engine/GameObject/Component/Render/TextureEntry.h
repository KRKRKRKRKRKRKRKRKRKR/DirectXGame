#pragma once
#include "../../../Graphics/Texture/TextureManager.h"
#include <string>

// テクスチャ選択コンボ（PlayScene::textures_、TextureSelectorComponent）で共有する
// 「テクスチャ1個ぶんのハンドルと表示名」のペア
struct TextureEntry {
	TextureHandle handle;
	std::string   name;
};
