#pragma once
#include "../../../Graphics/Texture/TextureManager.h"
#include <string>

// テクスチャ選択コンボ（PlayScene::textures_、TextureSelectorComponent）で共有する
// 「テクスチャ1個ぶんのハンドルと表示名」のペア
struct TextureEntry {
	TextureHandle handle;
	std::string   name;
};

// プロジェクトパネル→ヒエラルキー/インスペクターへの画像ドラッグ&ドロップ用ペイロードID。
// SceneBase.cppとModelRenderComponent.cppの両方から使うため、複数TUで共有できるここに置く
// （ImGuiのAcceptDragDropPayload/SetDragDropPayloadは文字列の中身で照合するため、
// TUをまたいでも同じ内容の文字列リテラルであれば問題なく一致する）
inline constexpr const char* kProjectImageDragDropId = "PROJECT_IMAGE_ASSET";
