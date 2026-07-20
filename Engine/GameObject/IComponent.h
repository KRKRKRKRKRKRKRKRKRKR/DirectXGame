#pragma once
#include "../../Math/MathTypes.h"
#include "../../Externals/Json/json.hpp"

class GameObject;

// GameObjectにアタッチする機能の最小抽象。描画は実際にはGame::Render()から
// Renderer経由で呼ぶ既存フローと衝突しないよう、Render相当のメソッドはここに含めない
// （描画コンポーネントは自身のDraw(Renderer*, ...)を独自に持つが、IComponent自体は関与しない）
class IComponent {
public:
	virtual ~IComponent() = default;

	// transformはオーナーGameObjectの実データへの参照（GameObject::Update()経由で渡される）。
	// GravityComponentのように毎フレーム位置を書き換えるコンポーネントはこれを直接編集する
	virtual void Update(float deltaTime, Transform& transform) { (void)deltaTime; (void)transform; }

	// オーナーのGameObjectが、Trigger相手と新しく重なった瞬間にColliderSystem::ResolveAndDraw
	// から呼ばれる（Unityの OnTriggerEnter(Collider other) と同じ役割）。重なり続けている間は
	// 呼ばれず、侵入した最初の1フレームだけ呼ばれる。デフォルトは何もしない＝当たり判定に
	// 反応する必要が無いコンポーネント（描画・ライト等）は無視してよい
	virtual void OnTriggerEnter(GameObject& other) { (void)other; }

	// 自分のImGui項目を描画する。デフォルトは何もしない（GameObject::DrawImGui()が
	// 型を気にせず全コンポーネントに呼ぶため、対応しないコンポーネントは無視される）
	virtual void DrawImGui(const char* namePrefix) { (void)namePrefix; }

	// 自分のフィールドをJSONへ書き出す/読み込む。デフォルトは何もしない（ComponentRegistryに
	// 登録していないコンポーネントは保存対象外になる。DrawImGuiと同じ「型を気にせず呼べる」設計）
	virtual void ToJson(nlohmann::json& out) const { (void)out; }
	virtual void FromJson(const nlohmann::json& in) { (void)in; }
};
