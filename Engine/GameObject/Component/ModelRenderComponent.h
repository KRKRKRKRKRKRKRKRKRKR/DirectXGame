#pragma once
#include "RenderComponentBase.h"

// Model(OBJ)とFBXModelの両方に使う。ModelHandleを先頭引数に取るRenderer::DrawModelを
// 呼ぶ点が他の描画コンポーネントと異なる。hasAnimationが真の場合のみ、Draw()内で
// UpdateModelAnimationを呼んでからDrawModelする（ボーンアニメーション付きFBXModel用）
class ModelRenderComponent : public RenderComponentBase {
public:
	ModelRenderComponent(Renderer::ModelHandle handle, bool hasAnimation)
		: modelHandle(handle), hasAnimation(hasAnimation) {}

	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const;

	Renderer::ModelHandle modelHandle;
	bool                   hasAnimation;
};
