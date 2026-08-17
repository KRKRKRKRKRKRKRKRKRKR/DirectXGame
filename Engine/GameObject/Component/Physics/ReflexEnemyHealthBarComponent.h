#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

class ReflexEnemyComponent;

// 敵の頭上にワールド空間でHPバーを描く見た目コンポーネント。TextureSelectorComponentと同じく
// 「同じGameObjectの兄弟コンポーネントへの生ポインタ」を持つ方式にする（IComponentはUpdate内で
// 自分のオーナーGameObjectを知る手段がなく、兄弟のReflexEnemyComponentからhp/maxHpを読むには
// このパターンしかないため）。コンストラクタ引数必須のためComponentRegistry::Registerで登録する
class ReflexEnemyHealthBarComponent : public IComponent {
public:
	explicit ReflexEnemyHealthBarComponent(ReflexEnemyComponent* target) : target_(target) {}

	// バーの見た目調整用（Inspectorから変更可能）
	float width = 1.2f;
	float height = 0.15f;
	float heightOffset = 1.0f; // 敵の中心からどれだけ上に浮かせるか
	Vector4 backgroundColor = { 0.15f, 0.15f, 0.15f, 0.8f };
	Vector4 fillColor = { 0.2f, 0.9f, 0.2f, 1.0f };

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	// targetは実行時にのみ有効な兄弟コンポーネントへの生ポインタのため保存しない
	// （TextureSelectorComponentのtarget_/textures_と同じ方針）。見た目パラメータのみ保存する
	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

private:
	ReflexEnemyComponent* target_;
};
