#pragma once
#include "../../Math/MathTypes.h"
#include "../../Externals/Json/json.hpp"
#include <vector>

class GameObject;
class Renderer;

// Update()に渡す、そのフレームの描画コンテキスト（ReflexPlayerComponentのように
// マウスクリック→ワールド座標変換にRenderer/view/projが必要なコンポーネント向け）。
// SceneBase::Renderがカメラのview/proj確定後に組み立ててGameObject::Updateへ渡す
struct UpdateContext {
	Renderer* renderer = nullptr;
	Matrix4x4 view;
	Matrix4x4 proj;

	// true: Gameビュー（配置カメラ視点、Gizmoなし）表示中。false: Sceneビュー（エディタ自由カメラ+Gizmo）表示中。
	// SceneBase::RenderがGizmoController::UpdatePicking等と同じ条件（useGameCamera）から設定する。
	// クリックでゲームロジックを動かすコンポーネント（ReflexPlayerComponent等）は、Sceneビュー表示中は
	// これを見て自分のクリック判定をスキップすることで、Gizmoのオブジェクト選択・矩形選択と
	// 同じ左クリックを取り合わないようにする
	bool isGameView = false;

	// シーン内の全GameObject一覧（SceneBase::gizmoTargets_と同じもの）への非所有ポインタ。
	// ReflexPlayerComponentが障害物（Colliderを持つGameObject）を探す等、シーン全体を
	// 参照したいコンポーネント向け。ライフタイムはこのUpdate呼び出しの間だけ有効
	const std::vector<GameObject*>* sceneObjects = nullptr;
};

// GameObjectにアタッチする機能の最小抽象。描画は実際にはGame::Render()から
// Renderer経由で呼ぶ既存フローと衝突しないよう、Render相当のメソッドはここに含めない
// （描画コンポーネントは自身のDraw(Renderer*, ...)を独自に持つが、IComponent自体は関与しない）
class IComponent {
public:
	virtual ~IComponent() = default;

	// transformはオーナーGameObjectの実データへの参照（GameObject::Update()経由で渡される）。
	// GravityComponentのように毎フレーム位置を書き換えるコンポーネントはこれを直接編集する。
	// ctxはクリック→ワールド座標変換等、Rendererのビューポート情報やview/projが必要な
	// コンポーネントのみ使う（大半のコンポーネントは無視してよい）
	virtual void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) { (void)deltaTime; (void)transform; (void)ctx; }

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
