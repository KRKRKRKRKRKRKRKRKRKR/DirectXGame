#pragma once

// GameObjectにアタッチする機能の最小抽象。描画は実際にはGame::Render()から
// Renderer経由で呼ぶ既存フローと衝突しないよう、Render相当のメソッドはここに含めない
// （描画コンポーネントは自身のDraw(Renderer*, ...)を独自に持つが、IComponent自体は関与しない）
class IComponent {
public:
	virtual ~IComponent() = default;
	virtual void Update(float deltaTime) { (void)deltaTime; }
};
