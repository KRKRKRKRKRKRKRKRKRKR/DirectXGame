#pragma once
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Graphics/Pipeline/BlendMode.h"
#include "../../../../Math/MathTypes.h"
#include "../../IComponent.h"

// Cube/Sphere/Triangle等、Rendererの「Transform+Color+Texture+Lighting+Blend+AlphaTest」
// という共通引数パターンを持つ描画コンポーネントの共通プロパティ置き場。
// Model系（ModelHandleが必要）やSprite系（UVTransformが必要）は派生側で追加メンバを持つ
class RenderComponentBase : public IComponent {
public:
	Vector4       color = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool          lighting = true;
	TextureHandle textureHandle = kTextureNone;
	BlendMode     blendMode = BlendMode::kNone;
	float         blendStrength = 1.0f;
	bool          alphaTest = false;
	float         alphaThreshold = 0.5f;

	// "{namePrefix} Lighting"/"{namePrefix} Color"/"{namePrefix} BlendMode"/
	// "{namePrefix} Blend Strength"/"{namePrefix} Alpha Test"を描画する
	void DrawImGui(const char* namePrefix) override;

	// textureHandleは実行のたびに変わる実行時ハンドルのため保存しない
	// （TextureSelectorComponent/MirrorComponentが自分の情報から毎回作り直す）
	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// PlayScene::Render()が具体型を気にせず汎用ループで描画できるようにするための仮想関数。
	// ModelRenderComponentだけdeltaTimeが必要なため、他の派生クラスは受け取るだけで使わない
	virtual void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const { (void)renderer; (void)transform; (void)deltaTime; }
};
