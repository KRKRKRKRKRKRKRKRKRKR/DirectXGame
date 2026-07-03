#pragma once
#include "RenderComponentBase.h"
#include "../GameObject.h"
#include "../../Graphics/Renderer/Renderer.h"

// GameObjectに付けるRenderComponentの種類。データ駆動生成（レベルファイル等から実行時に
// 種類を決めてコンポーネントを生成する）を見据えたenum。テンプレート引数は実行時の値を
// 受け取れないため、実行時のtype値からswitchで具体クラスを生成するCreateRenderComponent()と
// 組み合わせて使う
enum class RenderType {
	kCube,
	kSphere,
	kTriangle,
	kSprite,
	kModel,
};

// kSprite/kModelの生成時のみ使う追加情報。他のtypeでは無視される
struct RenderComponentDesc {
	bool                  is3D         = true;  // kSpriteのみ使用
	Renderer::ModelHandle modelHandle  = 0;      // kModelのみ使用
	bool                  hasAnimation = false;  // kModelのみ使用
};

// typeに応じた具体的なXxxRenderComponentをobjへ追加する。戻り値はRenderComponentBase*なので
// color/textureHandle等の共通プロパティはこの場で設定できるが、Draw()呼び出し時はこれまで通り
// GetComponent<具体型>()で取り出す必要がある（Draw()はIComponentの仮想関数ではないため）
RenderComponentBase* CreateRenderComponent(GameObject& obj, RenderType type, const RenderComponentDesc& desc = {});
