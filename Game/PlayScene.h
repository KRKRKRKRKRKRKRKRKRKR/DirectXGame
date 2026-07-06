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

	Sound bgm;

	struct TextureEntry {
		TextureHandle handle;
		std::string   name;
	};
	std::vector<TextureEntry> textures_;
	int sprite2DTexIndex_  = 0;
	int sprite3DTexIndex_  = 0;
	int triangleTexIndex_  = 0;
	int cubeTexIndex_      = 0;
	int sphereTexIndex_    = 0;
	int floorTexIndex_     = 0;
	int modelTexIndex_     = 0;
	int fbxModelTexIndex_  = 0;

	float triangleSmoothness_ = 1.0f;
	float cubeSmoothness_     = 1.0f;
	int   sphereSubdivision_  = static_cast<int>(Renderer::kSphereMaxSubdivision);

	// Blenderライクなギズモ操作：ImGuiで選んだ1オブジェクトのTransformをドラッグで編集する。
	// sprite2Dは対象外（スクリーン空間UIのため）。
	// PointLight/SpotLightはSceneLightのSetter経由でしか書き込めずTransformを持たないため、
	// lightGizmoScratch_という一時Transformを橋渡しに使う（UpdateGizmo()参照）。
	// 通常オブジェクト（GameObjectを持つもの）はenumのケースを手で並べず、gizmoTargets_という
	// 動的リストとgizmoTargetIndex_（リスト内インデックス）で選択する
	enum class GizmoTarget {
		kNone,
		kPointLight,
		kSpotLight,
	};
	GizmoTarget          gizmoTarget_    = GizmoTarget::kNone;
	ImGuizmo::OPERATION  gizmoOperation_ = ImGuizmo::TRANSLATE;

	// 通常オブジェクト（Transformを直接持つGameObject）の一覧。Initialize()末尾で
	// 全GameObjectメンバのアドレスを登録する。PointLight/SpotLightはGameObjectを
	// 持たない特殊ケースのため、このリストには含めずgizmoTarget_(enum)側で扱う
	std::vector<GameObject*> gizmoTargets_;

	// gizmoTargets_内で現在選択中のインデックス。-1は「このリストからは何も選んでいない」
	// （gizmoTarget_がkPointLight/kSpotLightの場合、またはkNoneの場合はこちらが有効になる）
	int gizmoTargetIndex_ = -1;

	// PointLight/SpotLightのギズモ操作を仲介する一時バッファ。SceneLightはTransform型を持たず
	// Setter経由でしか書き込めないため、ここに現在値をコピー→ImGuizmoで編集→差分をSetterへ書き戻す
	Transform lightGizmoScratch_;

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
