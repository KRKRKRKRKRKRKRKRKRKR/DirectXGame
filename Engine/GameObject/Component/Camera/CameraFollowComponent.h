#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

class GameObject;

// ターゲット（Race the Sunなら自動走行するプレイヤー）を追いかけるカメラ用コンポーネント。
// targetは保存されるGameObject参照ではなく、SceneBase::Renderが毎フレーム自動解決する実行時
// 専用フィールド：GameObject::tagが"Player"のオブジェクトを優先して選び、無ければシーン内で
// 最初に見つかったAutoRunComponent持ちのGameObjectにフォールバックする
// （CameraComponentのGameビュー選出＝"MainCamera"タグと同じ方針）
class CameraFollowComponent : public IComponent {
public:
	bool enabled = true;

	// ターゲットから見た相対位置（既定値は後方・少し上空）
	Vector3 offset = { 0.0f, 3.0f, -8.0f };

	// 追従の滑らかさ。大きいほど素早く追いつく。0以下にするとラグなしで即座に追従する
	float followLerp = 5.0f;

	bool lookAtTarget = true;

	// SceneBase::Renderが毎フレーム書き換える。未設定（追従対象のAutoRunComponent持ちオブジェクトが
	// シーンに無い）場合はUpdate()が何もしない
	GameObject* target = nullptr;

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	// targetはGameObject*のため保存しない（実行時にSceneBase::Renderが解決し直す）
	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
