#pragma once
#include "../../Math/MathTypes.h"

// GameObjectにアタッチする機能の最小抽象。描画は実際にはGame::Render()から
// Renderer経由で呼ぶ既存フローと衝突しないよう、Render相当のメソッドはここに含めない
// （描画コンポーネントは自身のDraw(Renderer*, ...)を独自に持つが、IComponent自体は関与しない）
class IComponent {
public:
	virtual ~IComponent() = default;

	// transformはオーナーGameObjectの実データへの参照（GameObject::Update()経由で渡される）。
	// GravityComponentのように毎フレーム位置を書き換えるコンポーネントはこれを直接編集する
	virtual void Update(float deltaTime, Transform& transform) { (void)deltaTime; (void)transform; }

	// 自分のImGui項目を描画する。デフォルトは何もしない（GameObject::DrawImGui()が
	// 型を気にせず全コンポーネントに呼ぶため、対応しないコンポーネントは無視される）
	virtual void DrawImGui(const char* namePrefix) { (void)namePrefix; }
};
