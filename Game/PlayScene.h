#pragma once
#include "IScene.h"
#include "../Engine/Graphics/Renderer/Renderer.h"
#include "../Engine/Graphics/Pipeline/BlendMode.h"
#include "../Engine/GameObject/Component/CollisionLayer.h"
#include "../Engine/Camera/Camera.h"
#include "../Engine/Audio/Sound.h"
#include "../Math/MathTypes.h"
#include "../Math/Collision.h"
#include "../Externals/imgui/imgui.h" // ImGuizmo.hがImDrawList等imgui型を前提にしており、先にインクルードする必要がある
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Engine/GameObject/GameObject.h"
#include "../Engine/GameObject/Component/CubeRenderComponent.h"
#include "../Engine/GameObject/Component/SphereRenderComponent.h"
#include "../Engine/GameObject/Component/TriangleRenderComponent.h"
#include "../Engine/GameObject/Component/ModelRenderComponent.h"
#include "../Engine/GameObject/Component/SpriteRenderComponent.h"
#include "../Engine/GameObject/Component/SphereColliderComponent.h"
#include "../Engine/GameObject/Component/OBBColliderComponent.h"
#include "../Engine/GameObject/Component/GravityComponent.h"
#include "../Engine/GameObject/Component/DirectionalLightComponent.h"
#include "../Engine/GameObject/Component/PointLightComponent.h"
#include "../Engine/GameObject/Component/SpotLightComponent.h"
#include "../Engine/GameObject/Component/TextureEntry.h"
#include "../Engine/GameObject/Component/TextureSelectorComponent.h"
#include <vector>
#include <string>

// ワールドの中身（GameObject群・当たり判定・Gizmo編集・ライティング）を保持する、
// ゲームプレイ画面用のIScene実装。Gameは「アプリの器」（Renderer/Cameraの橋渡し、FPS計測、
// FPS/Cameraパネル）に徹し、SceneManagerがシーン切り替えを担う。実際のシーン内容は
// すべてこちらに置く（Game.cppが肥大化したための分離、ロジックは無変更で移設したもの）
class PlayScene : public IScene {
public:
	void Initialize(Renderer* renderer, Camera* camera) override;

	// 内部でImGuizmo::BeginFrame()〜通常描画〜当たり判定〜ImGuiパネル描画までを一括して行う
	void Render(float deltaTime) override;

	SceneType GetNextScene() const override { return nextScene_; }

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;

	// 遷移先。デフォルトはkNone（遷移しない）で、デバッグ用のキー入力で書き換える
	SceneType nextScene_ = SceneType::kNone;

	// Unityの Play/Stop に相当する状態。falseの間（Stop＝編集モード）は
	// GameObject::Update()（GravityComponent等）とColliderの押し戻し解決を止め、
	// Gizmoで自由にオブジェクトを配置できるようにする（重なり検知・色分け表示自体は常時行う）。
	// デフォルトはfalse（シーンを開いた直後は編集モードから始まる、Unityと同じ挙動）
	bool isPlaying_ = false;

	// GameObject/コンポーネントシステムへの移行対象。CubeとFloorはプロパティ構成が
	// 完全に同一（Smoothness等の固有パラメータを持たない）ため、専用クラスを作らず
	// 同じCubeRenderComponent型を2個のGameObjectインスタンスで使い回している。
	// Sphere/TriangleはそれぞれSphereRenderComponent/TriangleRenderComponentという専用
	// クラスを持つ（Subdivision/Smoothnessはグローバル設定のためコンポーネントには持たせず、
	// cubeSmoothness_と同様にPlayScene側に据え置く）。Model(OBJ)/FBXModelはModelHandleを持つ
	// ModelRenderComponentを共用（hasAnimationフラグでボーンアニメーション更新の有無を切替）。
	// Sprite3D/Sprite2DはUVTransformを持つSpriteRenderComponentを共用（is3Dフラグで
	// DrawSprite3D/DrawSprite2Dのどちらを呼ぶか切替）。これで全9対象オブジェクトの移行が完了する
	GameObject cubeObject_;
	GameObject floorObject_;
	GameObject sphereObject_;
	GameObject triangleObject_;
	GameObject modelObject_;
	GameObject fbxModelObject_;
	GameObject sprite3DObject_;
	GameObject sprite2DObject_;

	// 鏡（平面反射）。floorObject_と同じ「薄いCubeを板として使う」パターンで表現する
	GameObject mirrorObject_;
	// mirrorObject_のTransformから反射平面（法線・原点からの距離）を導出する
	Collision::Plane ComputeMirrorPlane(const Transform& mirrorTransform) const;

	// ライト用の「空のGameObject」。Render/Colliderコンポーネントは持たず、対応する
	// XxxLightComponentのみを持つ。Transformが実在するため、他のオブジェクトと全く同じ
	// Gizmo選択・ドラッグ編集の仕組みに乗る（ライトだけの特殊分岐が不要になる）
	GameObject directionalLightObject_;
	GameObject pointLightObject_;
	GameObject spotLightObject_;

	Sound bgm;

	// テクスチャの選択インデックスは各オブジェクトが持つTextureSelectorComponentへ移った
	// （PlayScene側は共有一覧のtextures_のみを持つ）
	std::vector<TextureEntry> textures_;

	float triangleSmoothness_ = 1.0f;
	float cubeSmoothness_     = 1.0f;
	int   sphereSubdivision_  = static_cast<int>(Renderer::kSphereMaxSubdivision);

	// Blenderライクなギズモ操作：ImGuiで選んだ1オブジェクトのTransformをドラッグで編集する。
	// sprite2Dは対象外（スクリーン空間UIのため）。ライト用GameObjectも他と同じくTransformを
	// 実在させたため、通常オブジェクトと全く同じ形（gizmoTargets_＋gizmoTargetIndex_）で選択できる
	ImGuizmo::OPERATION  gizmoOperation_ = ImGuizmo::TRANSLATE;

	// 通常オブジェクト（Transformを直接持つGameObject）の一覧。Initialize()末尾で
	// 全GameObjectメンバのアドレスを登録する
	std::vector<GameObject*> gizmoTargets_;

	// gizmoTargets_内で現在選択中のインデックス。-1は「このリストからは何も選んでいない」
	int gizmoTargetIndex_ = -1;

	// "Objects"パネルに表示する全GameObjectの一覧（表示順）。gizmoTargets_とは目的が異なる
	// （Gizmo選択可否ではなくObjectsパネル表示可否）ため独立して持つ。Sprite2DはgizmoTargets_には
	// 含まれないがこちらには含まれる
	std::vector<GameObject*> objectPanelTargets_;

	// "Gizmo"パネルの"Edit Collider"チェックボックス。オンの間、ギズモの対象は選択中
	// GameObjectのTransformではなく、そのGameObjectが持つCollider（オフセット+サイズ）に切り替わる
	bool editCollider_ = false;

	// Collider編集を仲介する一時バッファ。translationはワールド座標に変換したコライダー中心、
	// scaleはSphereなら{radius,radius,radius}、Boxならhalfsize*2を格納する。rotationは使わない
	Transform colliderGizmoScratch_;

	// マウスピッキング：3Dビュー上で左クリックした瞬間を検知するための前フレーム状態。
	// InputDeviceは左クリックの「押されているか」のみでトリガー版を持たないため、ここで保持する
	bool prevMouseLeftPressed_ = false;

	Transform* GetGizmoTargetTransform();
	void       UpdateGizmo(const Matrix4x4& view, const Matrix4x4& proj);

	// gizmoTargets_内のオブジェクトをTransform.scaleから概算したBounding Sphereとみなし、
	// 左クリック位置から飛ばしたレイとの交差判定で最も手前のものをギズモ選択状態に反映する。
	// ImGuizmo操作中/ImGuiパネル操作中は発火しない（既存のコンボボックス選択と共存する追加手段）
	void UpdatePicking(const Matrix4x4& view, const Matrix4x4& proj);

	// gizmoTargets_内でCollider（Sphere/Box）を持つ全オブジェクトについて、
	// ColliderComponentBase::ShouldLayersCollide（お互いが相手の所属レイヤーを自分の
	// collidesWithに含めている場合のみtrue）を通過したペアの重なりを判定する。両方Solid（isTrigger=false）で重なっている場合は
	// Collision::*Penetrationで押し戻し量を求め、Transform.translationを半分ずつ動かして分離する
	// （isTriggerがどちらかtrueなら押し戻さず検知のみ）。判定・押し戻し後の位置でワイヤーフレームを
	// 緑（重なりなし）/黄（Trigger込みの重なり）/赤（Solid同士の重なり）に色分けして描画する
	void ResolveAndDrawColliderGizmos(const Matrix4x4& view, const Matrix4x4& proj);

	void DrawGrid();
	void DrawImGui();
};
