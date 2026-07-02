#pragma once
#include "RenderComponentBase.h"

class SphereRenderComponent : public RenderComponentBase {
public:
	void Draw(Renderer* renderer, const Transform& transform) const;
};
