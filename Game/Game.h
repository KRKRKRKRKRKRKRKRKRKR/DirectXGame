#pragma once
#include "../Engine/Graphics/Renderer/Renderer.h"
#include "../Engine/Graphics/Pipeline/BlendMode.h"
#include "../Engine/Camera/Camera.h"
#include "../Engine/Audio/Sound.h"
#include "../Math/MathTypes.h"
#include "../Math/Collision.h"
#include "../Externals/imgui/imgui.h" // ImGuizmo.hがImDrawList等imgui型を前提にしており、先にインクルードする必要がある
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
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

	Transform sphere;
	Transform cube;
	Transform triangle;
	Transform sprite3D;
	Transform sprite2D;
	Transform floor_;

	UVTransform sprite2DUV;
	UVTransform sprite3DUV;

	Vector4 sphereColor   = { 1,1,1,1 };
	Vector4 cubeColor     = { 1,1,1,1 };
	Vector4 triangleColor = { 1,1,1,1 };
	Vector4 sprite3DColor = { 1,1,1,1 };
	Vector4 sprite2DColor = { 1,1,1,1 };
	Vector4 floorColor_   = { 1,1,1,1 };

	Sound bgm;

	Renderer::ModelHandle modelHandle_ = 0;
	Transform             modelTransform_;
	Vector4               modelColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool                  modelLighting_ = true;
	int                   modelTexIndex_ = 0;

	// Assimp導入確認用（FBX読み込みテスト）
	Renderer::ModelHandle fbxModelHandle_ = 0;
	Transform             fbxModelTransform_;
	Vector4               fbxModelColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool                  fbxModelLighting_ = true;
	int                   fbxModelTexIndex_ = 0;

	bool sphereLighting   = true;
	bool cubeLighting     = true;
	bool triangleLighting = true;
	bool sprite3DLighting = true;
	bool sprite2DLighting = false;
	bool floorLighting_   = true;

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
	BlendMode triangleBlendMode_ = BlendMode::kNone;
	BlendMode cubeBlendMode_     = BlendMode::kNone;
	BlendMode sphereBlendMode_   = BlendMode::kNone;
	BlendMode modelBlendMode_    = BlendMode::kNone;
	BlendMode sprite3DBlendMode_ = BlendMode::kNone;
	BlendMode sprite2DBlendMode_ = BlendMode::kNone;
	BlendMode floorBlendMode_    = BlendMode::kNone;
	BlendMode fbxModelBlendMode_ = BlendMode::kNone;

	// OMSetBlendFactorに渡す0〜1の強さ。kNormal/kAdd/kSubtractのみ効果がある
	float gridCubeBlendStrength_ = 1.0f;
	float triangleBlendStrength_ = 1.0f;
	float cubeBlendStrength_     = 1.0f;
	float sphereBlendStrength_   = 1.0f;
	float modelBlendStrength_    = 1.0f;
	float sprite3DBlendStrength_ = 1.0f;
	float sprite2DBlendStrength_ = 1.0f;
	float floorBlendStrength_    = 1.0f;
	float fbxModelBlendStrength_ = 1.0f;

	// 2値抜き(Binary Alpha/αTest)。αがしきい値未満のピクセルをdiscardする
	bool  gridCubeAlphaTest_  = false;
	bool  triangleAlphaTest_  = false;
	bool  cubeAlphaTest_      = false;
	bool  sphereAlphaTest_    = false;
	bool  modelAlphaTest_     = false;
	bool  sprite3DAlphaTest_  = false;
	bool  sprite2DAlphaTest_  = false;
	bool  floorAlphaTest_     = false;
	bool  fbxModelAlphaTest_  = false;

	float gridCubeAlphaThreshold_  = 0.5f;
	float triangleAlphaThreshold_  = 0.5f;
	float cubeAlphaThreshold_      = 0.5f;
	float sphereAlphaThreshold_    = 0.5f;
	float modelAlphaThreshold_     = 0.5f;
	float sprite3DAlphaThreshold_  = 0.5f;
	float sprite2DAlphaThreshold_  = 0.5f;
	float floorAlphaThreshold_     = 0.5f;
	float fbxModelAlphaThreshold_  = 0.5f;

	// FPS表示（負荷テスト用）。0.5秒ごとに直近フレームの平均FPS/フレーム時間を更新する
	// （毎フレームの値は変動が激しく読みづらいため、一定間隔でサンプリングして表示する）
	float fpsSampleTimer_    = 0.0f;
	int   fpsSampleFrames_   = 0;
	float fpsDisplayValue_   = 0.0f;
	float frameTimeDisplayMs_ = 0.0f;

	// Blenderライクなギズモ操作：ImGuiで選んだ1オブジェクトのTransformをドラッグで編集する。
	// gridCubes_/sprite2Dは対象外（gridCubes_は9万個規模で個別編集不可、sprite2Dはスクリーン空間UI）。
	// PointLight/SpotLightはSceneLightのSetter経由でしか書き込めずTransformを持たないため、
	// lightGizmoScratch_という一時Transformを橋渡しに使う（UpdateGizmo()参照）
	enum class GizmoTarget {
		kNone,
		kCube,
		kSphere,
		kTriangle,
		kFloor,
		kSprite3D,
		kModel,
		kFbxModel,
		kPointLight,
		kSpotLight,
	};
	GizmoTarget          gizmoTarget_    = GizmoTarget::kNone;
	ImGuizmo::OPERATION  gizmoOperation_ = ImGuizmo::TRANSLATE;

	// PointLight/SpotLightのギズモ操作を仲介する一時バッファ。SceneLightはTransform型を持たず
	// Setter経由でしか書き込めないため、ここに現在値をコピー→ImGuizmoで編集→差分をSetterへ書き戻す
	Transform lightGizmoScratch_;

	Transform* GetGizmoTargetTransform();
	void       UpdateGizmo(const Matrix4x4& view, const Matrix4x4& proj);

	void DrawGrid();
	void DrawImGui();
};
