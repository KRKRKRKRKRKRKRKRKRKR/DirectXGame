#pragma once
#include "RenderComponentBase.h"

class CubeRenderComponent : public RenderComponentBase {
public:
	void Draw(Renderer* renderer, const Transform& transform) const;
};
