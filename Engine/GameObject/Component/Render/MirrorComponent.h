#pragma once
#include "../../IComponent.h"
#include "CubeRenderComponent.h"
#include "../../../../Math/Collision.h"

class Renderer;

// 鏡面反射オブジェクトに付与するコンポーネント。対象のCubeRenderComponentへの生ポインタを
// 持ち（TextureSelectorComponentと同じ構造）、自分では描画せず委譲する
class MirrorComponent : public IComponent {
public:
	explicit MirrorComponent(CubeRenderComponent* render) : render_(render) {}

	// 自身のTransform（回転・位置）から反射平面（法線・原点からの距離）を導出する。
	// 鏡面オブジェクトの前面（Cube.cppのZ+面）をローカル法線として使う
	Collision::Plane ComputePlane(const Transform& ownerTransform) const;

	// 反射パスで描いたテクスチャを自分のtextureHandleに差し替えてから、
	// 対象のCubeRenderComponentに描画を委譲する
	void Draw(Renderer* renderer, const Transform& ownerTransform) const;

private:
	CubeRenderComponent* render_;
};
