#pragma once
#include "../IComponent.h"
#include "TextureEntry.h"
#include <vector>

class RenderComponentBase;

// RenderComponentBase系コンポーネントに「テクスチャ選択コンボ」を追加で持たせるコンポーネント。
// 対象コンポーネントへの生ポインタと、共有テクスチャ一覧（PlayScene::textures_）への
// 参照を持ち、DrawImGuiでコンボを描画し、選択結果を毎回target_->textureHandleへ反映する
class TextureSelectorComponent : public IComponent {
public:
	TextureSelectorComponent(RenderComponentBase* target, const std::vector<TextureEntry>* textures, int initialIndex = 0)
		: target_(target), textures_(textures), index_(initialIndex) {
	}

	void DrawImGui(const char* namePrefix) override;

private:
	RenderComponentBase* target_;
	const std::vector<TextureEntry>* textures_;
	int index_;
};
