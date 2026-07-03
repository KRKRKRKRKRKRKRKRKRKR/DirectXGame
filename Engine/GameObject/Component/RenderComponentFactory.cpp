#include "RenderComponentFactory.h"
#include "CubeRenderComponent.h"
#include "SphereRenderComponent.h"
#include "TriangleRenderComponent.h"
#include "SpriteRenderComponent.h"
#include "ModelRenderComponent.h"

RenderComponentBase* CreateRenderComponent(GameObject& obj, RenderType type, const RenderComponentDesc& desc) {
	switch (type) {
	case RenderType::kCube:
		return obj.AddComponent<CubeRenderComponent>();
	case RenderType::kSphere:
		return obj.AddComponent<SphereRenderComponent>();
	case RenderType::kTriangle:
		return obj.AddComponent<TriangleRenderComponent>();
	case RenderType::kSprite:
		return obj.AddComponent<SpriteRenderComponent>(desc.is3D);
	case RenderType::kModel:
		return obj.AddComponent<ModelRenderComponent>(desc.modelHandle, desc.hasAnimation);
	}
	return nullptr;
}
