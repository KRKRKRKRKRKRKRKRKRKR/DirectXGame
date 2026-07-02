#pragma once
#include "../Engine/Graphics/Renderer/Renderer.h"
#include "../Engine/Graphics/Pipeline/BlendMode.h"
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
class Game {
public:
	Game() = default;
	~Game() = default;

	void Initialize(Renderer* renderer, Camera* camera);
	void Update(float deltaTime);
	void Render();

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	float deltaTime_ = 0.0f;

	// GameObject/コンポーネントシステムへの移行対象。CubeとFloorはプロパティ構成が
	// 完全に同一（Smoothness等の固有パラメータを持たない）ため、専用クラスを作らず
	// 同じCubeRenderComponent型を2個のGameObjectインスタンスで使い回している。
	// Sphere/TriangleはそれぞれSphereRenderComponent/TriangleRenderComponentという専用
	// クラスを持つ（Subdivision/Smoothnessはグローバル設定のためコンポーネントには持たせず、
	// cubeSmoothness_と同様にGame側に据え置く）。Model(OBJ)/FBXModelはModelHandleを持つ
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

	Sound bgm;

	std::vector<Transform> gridCubes_;

	// 負荷テスト用：立方体状グリッドの1辺の個数（個数はgridSize_の3乗）。
	// ImGuiで変更すると次フレームにgridCubes_を作り直す（Cube::kMaxInstanceCountの上限まで試せる）
	int gridSize_ = 30;
	static constexpr int kGridSizeMax = 48; // 48^3=110592。Cube::kMaxInstanceCount(131072)以内
	void RebuildGridCubes();

	// フラストラムカリング：視錐台の外にあるグリッドCubeのDrawCube呼び出し自体をスキップする。
	// CPU側の毎フレームコスト（行列計算・SetWvpMatrix・Root Constant設定）を削減するのが目的
	bool  gridFrustumCullingEnabled_ = true;
	int   gridCubesDrawnCount_       = 0; // 直近フレームで実際にDrawCubeした個数（ImGui表示用）

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
	int gridCubeTexIndex_  = 0;
	int floorTexIndex_     = 0;
	int modelTexIndex_     = 0;
	int fbxModelTexIndex_  = 0;

	Vector4 gridCubeColor_    = { 1,1,1,1 };
	bool    gridCubeLighting_ = true;
	Vector3 gridCubeRotation_ = { 0.0f, 0.0f, 0.0f };

	// グリッドは全個体が同じ回転（位置のみ個体ごとに違う）。WorldInverseTransposeは
	// 回転成分だけで決まるため、回転が変化した時だけ再計算してキャッシュする
	Vector3   gridCubeCachedRotation_{ 0.0f, 1.0f, 0.0f }; // 初期値をgridCubeRotation_と異なる値にして初回必ず計算させる
	Matrix4x4 gridCubeWorldInverseTranspose_{};

	float triangleSmoothness_ = 1.0f;
	float cubeSmoothness_     = 1.0f;
	int   sphereSubdivision_  = static_cast<int>(Renderer::kSphereMaxSubdivision);

	BlendMode gridCubeBlendMode_ = BlendMode::kNone;

	// OMSetBlendFactorに渡す0〜1の強さ。kNormal/kAdd/kSubtractのみ効果がある
	float gridCubeBlendStrength_ = 1.0f;

	// 2値抜き(Binary Alpha/αTest)。αがしきい値未満のピクセルをdiscardする
	bool  gridCubeAlphaTest_  = false;

	float gridCubeAlphaThreshold_  = 0.5f;

	// FPS表示（負荷テスト用）。0.5秒ごとに直近フレームの平均FPS/フレーム時間を更新する
	// （毎フレームの値は変動が激しく読みづらいため、一定間隔でサンプリングして表示する）
	float fpsSampleTimer_    = 0.0f;
	int   fpsSampleFrames_   = 0;
	float fpsDisplayValue_   = 0.0f;
	float frameTimeDisplayMs_ = 0.0f;

	// Blenderライクなギズモ操作：ImGuiで選んだ1オブジェクトのTransformをドラッグで編集する。
	// gridCubes_/sprite2Dは対象外（gridCubes_は9万個規模で個別編集不可、sprite2Dはスクリーン空間UI）。
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

	// gizmoTargets_内でCollider（Sphere/Box）を持つ全オブジェクトをワイヤーフレームで可視化する。
	// 他の少なくとも1つのColliderと重なっていれば赤、していなければ緑で表示する
	void DrawColliderGizmos(const Matrix4x4& view, const Matrix4x4& proj);

	void DrawGrid();
	void DrawImGui();
};
