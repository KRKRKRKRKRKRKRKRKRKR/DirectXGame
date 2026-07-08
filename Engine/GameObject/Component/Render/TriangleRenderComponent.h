#pragma once
#include "RenderComponentBase.h"

class TriangleRenderComponent : public RenderComponentBase {
public:
	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;
};
