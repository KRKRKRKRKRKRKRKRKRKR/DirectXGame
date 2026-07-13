#pragma once
#include "../../IComponent.h"
#include "RenderComponentBase.h"
#include "TextureEntry.h"
#include <vector>

// RenderComponentBase系コンポーネントに「テクスチャ選択コンボ」を追加で持たせるコンポーネント。
// 対象コンポーネントへの生ポインタと、共有テクスチャ一覧（PlayScene::textures_）への
// 参照を持ち、DrawImGuiでコンボを描画し、選択結果を毎回target_->textureHandleへ反映する
class TextureSelectorComponent : public IComponent {
public:
	TextureSelectorComponent(RenderComponentBase* target, const std::vector<TextureEntry>* textures, int initialIndex = 0)
		: target_(target), textures_(textures), index_(initialIndex) {
		// DrawImGuiは選択中オブジェクトのみ毎フレーム呼ばれる想定のため、生成直後（未選択の間）は
		// 一度も呼ばれない場合がある。ここでも反映しておかないとtextureHandleが初期値(白)のまま残る
		target_->textureHandle = (*textures_)[index_].handle;
	}

	void DrawImGui(const char* namePrefix) override;

	// インデックスではなく名前（textures_[index_].name）を書き出す。読み込み順が変わっても
	// 壊れないようにするため（復元はComponentRegistryのcreatorがこの名前から現在のindexを引き直す）
	void ToJson(nlohmann::json& out) const override {
		out["textureName"] = (*textures_)[index_].name;
	}

private:
	RenderComponentBase* target_;
	const std::vector<TextureEntry>* textures_;
	int index_;
};
