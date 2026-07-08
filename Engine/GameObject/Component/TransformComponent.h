#pragma once
#include "../IComponent.h"
#include "../../../Math/MathTypes.h"

// GameObjectが必ず1つ持つTransform（scale/rotation/translation）をコンポーネント化したもの。
// GameObjectのコンストラクタで自動的にAddComponentされ、GameObject::GetTransform()経由でアクセスする
class TransformComponent : public IComponent {
public:
	Transform transform;

	// ImGuiドラッグの刻み幅・範囲。オブジェクトごとに見た目のスケール感が異なるため
	// PlayScene::Initialize側で個別に上書きできるようにする（デフォルトは最も一般的な値）
	float scaleSpeed = 0.01f, scaleMin = 0.1f, scaleMax = 10.0f;
	float rotationSpeed = 0.01f; // 範囲は常に±π固定（回転に単位変換の違いがないため）
	float translationSpeed = 0.01f, translationMin = -10.0f, translationMax = 10.0f;

	// true: Sprite2D用の2D表示（Pos(px)/Size(px)+Z軸回転のみ、DragFloat2を使う）にする
	bool is2D = false;

	void DrawImGui(const char* namePrefix) override;
};
