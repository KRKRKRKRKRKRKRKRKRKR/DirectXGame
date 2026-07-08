#include "PlayScene.h"
#include "../Externals/imgui/imgui.h"
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Math/VectorMath.h"
#include "../Math/JsonUtil.h"
#include "../Engine/Audio/AudioManager.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <fstream>

// 方向ベクトル→オイラー角(ラジアン)。Initialize()でDirectional/Spot Lightの初期回転を
// デフォルト方向値から逆算するためだけに使う
static Vector3 DirectionToEulerRadians(const Vector3& direction) {
	using namespace DirectX;
	XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&direction));
	XMVECTOR baseDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	float dot = XMVectorGetX(XMVector3Dot(baseDir, dir));
	dot = std::clamp(dot, -1.0f, 1.0f);
	XMVECTOR axis = XMVector3Cross(baseDir, dir);
	XMVECTOR quat;
	if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-8f) {
		// 平行または反対向き：外積が定義できないので特別扱い
		quat = (dot > 0.0f) ? XMQuaternionIdentity()
		                     : XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XM_PI);
	} else {
		quat = XMQuaternionRotationAxis(XMVector3Normalize(axis), acosf(dot));
	}
	Matrix4x4 rotMat;
	XMStoreFloat4x4(&rotMat, XMMatrixRotationQuaternion(quat));

	// 既存のMakeAffineMatrix/Decomposeと同じ経路（行列→DecomposeMatrixToComponents）で変換する
	float t[3], r[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents(&rotMat._11, t, r, s);
	return {
		XMConvertToRadians(r[0]),
		XMConvertToRadians(r[1]),
		XMConvertToRadians(r[2]) };
}

// ---- "Create"ボタン用のアーキタイプ（コンポーネント構成のひな形）----
// 既存のJSON復元パイプライン（ComponentRegistry＋GameObject::FromJson）にそのまま流し込める
// 形で用意する。Model/Sprite/AudioSourceはファイルパス等の追加入力が要るため対象外とし、
// 値だけで完結する種類だけを対象にする

static nlohmann::json MakeCubeArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });

	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json render;
	render["type"] = "CubeRender";
	render["data"] = nlohmann::json::object();
	comps.push_back(render);

	nlohmann::json texSelector;
	texSelector["type"] = "TextureSelector";
	texSelector["data"]["textureName"] = "t.png";
	comps.push_back(texSelector);

	nlohmann::json collider;
	collider["type"] = "OBBCollider";
	collider["data"]["halfSize"] = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	comps.push_back(collider);

	j["components"] = comps;
	return j;
}

static nlohmann::json MakeSphereArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });

	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json render;
	render["type"] = "SphereRender";
	render["data"] = nlohmann::json::object();
	comps.push_back(render);

	nlohmann::json texSelector;
	texSelector["type"] = "TextureSelector";
	texSelector["data"]["textureName"] = "t.png";
	comps.push_back(texSelector);

	nlohmann::json collider;
	collider["type"] = "SphereCollider";
	collider["data"]["radius"] = 1.0f;
	comps.push_back(collider);

	j["components"] = comps;
	return j;
}

static nlohmann::json MakeTriangleArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });

	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json render;
	render["type"] = "TriangleRender";
	render["data"] = nlohmann::json::object();
	comps.push_back(render);

	nlohmann::json texSelector;
	texSelector["type"] = "TextureSelector";
	texSelector["data"]["textureName"] = "t.png";
	comps.push_back(texSelector);

	nlohmann::json collider;
	collider["type"] = "SphereCollider";
	collider["data"]["radius"] = 1.0f;
	comps.push_back(collider);

	j["components"] = comps;
	return j;
}

static nlohmann::json MakeDirectionalLightArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 15.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson(DirectionToEulerRadians({ 0.0f, -1.0f, 0.0f }));
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json light;
	light["type"] = "DirectionalLight";
	light["data"] = nlohmann::json::object();
	comps.push_back(light);
	j["components"] = comps;
	return j;
}

static nlohmann::json MakePointLightArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 2.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json light;
	light["type"] = "PointLight";
	light["data"] = nlohmann::json::object();
	comps.push_back(light);
	j["components"] = comps;
	return j;
}

static nlohmann::json MakeSpotLightArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 2.0f, 3.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson(DirectionToEulerRadians({ -1.0f, -1.0f, 0.0f }));
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json light;
	light["type"] = "SpotLight";
	light["data"] = nlohmann::json::object();
	comps.push_back(light);
	j["components"] = comps;
	return j;
}

// "Create"コンボに並ぶアーキタイプ名の一覧。名前からひな形JSONを引く
static const std::vector<std::string> kArchetypeNames = {
	"Cube", "Sphere", "Triangle", "Directional Light", "Point Light", "Spot Light",
};

static nlohmann::json GetArchetypeJson(const std::string& name) {
	if (name == "Cube")              return MakeCubeArchetype();
	if (name == "Sphere")            return MakeSphereArchetype();
	if (name == "Triangle")          return MakeTriangleArchetype();
	if (name == "Directional Light") return MakeDirectionalLightArchetype();
	if (name == "Point Light")       return MakePointLightArchetype();
	if (name == "Spot Light")        return MakeSpotLightArchetype();
	return nlohmann::json::object();
}

GameObject& PlayScene::CreateObject(const std::string& name) {
	auto obj = std::make_unique<GameObject>();
	obj->name = name;
	GameObject& ref = *obj;
	objects_.push_back(std::move(obj));
	return ref;
}

GameObject& PlayScene::CreateObjectFromArchetype(const std::string& archetypeName) {
	// 表示上の重複を避けるための簡易な連番付け（Save/LoadはもうGameObjectの名前一致に
	// 依存していないため、多少重複してもロード等の正しさには影響しない）
	std::string uniqueName = archetypeName + " " + std::to_string(objects_.size() + 1);
	GameObject& obj = CreateObject(uniqueName);
	ComponentLoadContext ctx{ renderer_, &textures_ };
	obj.FromJson(GetArchetypeJson(archetypeName), ctx);
	return obj;
}

void PlayScene::DeleteSelectedObject() {
	GameObject* selected = gizmoController_.GetSelected(gizmoTargets_);
	if (!selected) return;

	auto it = std::find_if(objects_.begin(), objects_.end(),
		[selected](const std::unique_ptr<GameObject>& obj) { return obj.get() == selected; });
	if (it != objects_.end()) objects_.erase(it);

	RebuildDerivedLists();
	gizmoController_.ResetSelection();
}

void PlayScene::RebuildDerivedLists() {
	gizmoTargets_.clear();
	objectPanelTargets_.clear();
	for (auto& obj : objects_) {
		if (!obj->excludeFromGizmoList) gizmoTargets_.push_back(obj.get());
		if (!obj->excludeFromObjectPanel) objectPanelTargets_.push_back(obj.get());
	}
}

void PlayScene::Initialize(Renderer* renderer, Camera* camera) {
	renderer_ = renderer;
	camera_ = camera;
	nextScene_ = SceneType::kNone;

	// テクスチャ一覧をまとめてロード（"なし" は白テクスチャ）
	textures_.push_back({ kTextureNone, "なし" });
	for (const std::string& path : std::vector<std::string>{
		"Resources/t.png",
		"Resources/f.png",
		"Resources/s.png",
		"Resources/monsterBall.png.png",
		"Resources/White.png",
		"Resources/transparent .png", // αTest確認用（板の隙間が透過になっている柵）
		}) {
		TextureHandle h = renderer_->LoadTexture(path);
		textures_.push_back({ h, path.substr(path.find_last_of('/') + 1) });
	}

	// 以下、"Objects"パネルに表示したい順でCreateObjectする（gizmoTargets_/objectPanelTargets_は
	// この生成順をそのまま反映するため）

	GameObject& floorObject = CreateObject("Floor");
	floorObject.GetTransform().translation = { 0.0f, -0.5f, 0.0f };
	floorObject.GetTransform().scale       = { 100.0f, 0.01f, 100.0f };
	// scale基準のBounding SphereだとFloorが巨大になり誤検出するため除外（コンボからの選択は可能）
	floorObject.excludeFromPicking = true;
	CubeRenderComponent* floorRender = floorObject.AddComponent<CubeRenderComponent>();
	floorRender->textureHandle = textures_[1].handle;
	floorObject.AddComponent<TextureSelectorComponent>(floorRender, &textures_, 1);

	// Y方向だけ厚い判定ボリュームにして、GravityComponentの高速落下によるすり抜けを防ぐ
	OBBColliderComponent* floorCollider = floorObject.AddComponent<OBBColliderComponent>();
	floorCollider->halfSize = { 1.0f, 100.0f, 1.0f };
	floorCollider->offset   = { 0.0f, -0.99f, 0.0f };
	floorCollider->layer = CollisionLayer::kEnvironment;
	floorCollider->isStatic = true; // 背景系は押し戻されないようにする

	// Floorは巨大なオブジェクトなのでSRTのドラッグ刻み幅・範囲を広くする
	TransformComponent* floorTransform = floorObject.GetComponent<TransformComponent>();
	floorTransform->scaleSpeed = 0.1f; floorTransform->scaleMin = 0.1f; floorTransform->scaleMax = 200.0f;
	floorTransform->translationSpeed = 0.1f; floorTransform->translationMin = -100.0f; floorTransform->translationMax = 100.0f;

	GameObject& triangleObject = CreateObject("Triangle");
	triangleObject.GetTransform().translation = { 0.0f, 1.0f, 0.0f };
	TriangleRenderComponent* triangleRender = triangleObject.AddComponent<TriangleRenderComponent>();
	triangleRender->textureHandle = textures_[1].handle;
	triangleObject.AddComponent<TextureSelectorComponent>(triangleRender, &textures_, 1);

	// Triangle専用Colliderが無いためSphereColliderComponentで近似
	SphereColliderComponent* triangleCollider = triangleObject.AddComponent<SphereColliderComponent>();
	triangleCollider->radius = 1.0f;
	triangleCollider->layer = CollisionLayer::kDefault;

	GameObject& cubeObject = CreateObject("Cube");
	cubeObject.GetTransform().translation = { -3.0f, 1.0f, 0.0f };
	CubeRenderComponent* cubeRender = cubeObject.AddComponent<CubeRenderComponent>();
	cubeRender->textureHandle = textures_[1].handle; // 初期テクスチャ: t.png
	cubeObject.AddComponent<TextureSelectorComponent>(cubeRender, &textures_, 1);

	// OBB（回転追従）を使用。Edit Colliderでオフセット・サイズをドラッグ調整できる
	OBBColliderComponent* cubeCollider = cubeObject.AddComponent<OBBColliderComponent>();
	cubeCollider->halfSize = { 1.0f, 1.0f, 1.0f };
	cubeCollider->layer = CollisionLayer::kObstacle;
	cubeCollider->collidesWith[static_cast<size_t>(CollisionLayer::kObstacle)] = false; // 障害物同士は判定しない
	cubeCollider->collidesWith[static_cast<size_t>(CollisionLayer::kItem)]     = false; // 障害物とアイテムは判定しない

	// CubeはSolidでEnvironment(Floor)と衝突するため、そのままGravityComponentの動作確認に使える
	cubeObject.AddComponent<GravityComponent>();

	GameObject& sphereObject = CreateObject("Sphere");
	sphereObject.GetTransform().translation = { 3.0f, 1.0f, 0.0f };
	SphereRenderComponent* sphereRender = sphereObject.AddComponent<SphereRenderComponent>();
	sphereRender->textureHandle = textures_[1].handle;
	sphereObject.AddComponent<TextureSelectorComponent>(sphereRender, &textures_, 1);

	SphereColliderComponent* sphereCollider = sphereObject.AddComponent<SphereColliderComponent>();
	sphereCollider->radius = 1.0f;
	sphereCollider->layer = CollisionLayer::kItem;
	sphereCollider->isTrigger = true;
	sphereCollider->collidesWith[static_cast<size_t>(CollisionLayer::kObstacle)]    = false;
	sphereCollider->collidesWith[static_cast<size_t>(CollisionLayer::kItem)]        = false;
	sphereCollider->collidesWith[static_cast<size_t>(CollisionLayer::kEnvironment)] = false;

	GameObject& modelObject = CreateObject("Model (OBJ)");
	modelObject.GetTransform().translation = { 5.0f, 0.0f, 0.0f };
	// メッシュ実寸がCubeと異なるためpickingRadiusHintを個別調整
	modelObject.pickingRadiusHint = 2.0f;
	ModelRenderComponent* modelRender = modelObject.AddComponent<ModelRenderComponent>(
		renderer_->LoadModel("Resources/Model", "player.obj"), /*hasAnimation=*/false);
	modelRender->directoryPath = "Resources/Model"; modelRender->filename = "player.obj"; // JSON保存用
	modelRender->textureHandle = textures_[1].handle; // 初期テクスチャ: t.png
	modelObject.AddComponent<TextureSelectorComponent>(modelRender, &textures_, 1);

	SphereColliderComponent* modelCollider = modelObject.AddComponent<SphereColliderComponent>();
	modelCollider->radius = modelObject.pickingRadiusHint;
	modelCollider->layer = CollisionLayer::kDefault;

	// FBX(Assimp)読み込み・ボーンアニメーションの確認用
	GameObject& fbxModelObject = CreateObject("FBX Model");
	fbxModelObject.GetTransform().translation = { 8.0f, 0.0f, 0.0f };
	// FBXのUnitScaleFactorが実寸(cm相当)のため0.01倍で単位を合わせる
	fbxModelObject.GetTransform().scale = { 0.01f, 0.01f, 0.01f };
	// pickingRadiusHint*scaleが概ね1.0mになるよう逆算した値
	fbxModelObject.pickingRadiusHint = 100.0f;
	ModelRenderComponent* fbxModelRender = fbxModelObject.AddComponent<ModelRenderComponent>(
		renderer_->LoadModel("Resources/Model", "HumanModel_ver2.fbx"), /*hasAnimation=*/true);
	fbxModelRender->directoryPath = "Resources/Model"; fbxModelRender->filename = "HumanModel_ver2.fbx"; // JSON保存用
	fbxModelRender->textureHandle = textures_[0].handle; // 現状未選択(なし)のまま据え置き
	fbxModelObject.AddComponent<TextureSelectorComponent>(fbxModelRender, &textures_, 0);

	// radiusはscale非依存の絶対値のためpickingRadiusHint*scaleで換算
	SphereColliderComponent* fbxModelCollider = fbxModelObject.AddComponent<SphereColliderComponent>();
	fbxModelCollider->radius = fbxModelObject.pickingRadiusHint * fbxModelObject.GetTransform().scale.x;
	fbxModelCollider->layer = CollisionLayer::kDefault;

	// FBX Modelは実寸(cm相当)にscale=0.01を掛けている関係でscaleの刻み幅を細かくする
	TransformComponent* fbxModelTransform = fbxModelObject.GetComponent<TransformComponent>();
	fbxModelTransform->scaleSpeed = 0.001f; fbxModelTransform->scaleMin = 0.001f; fbxModelTransform->scaleMax = 1.0f;
	fbxModelTransform->translationSpeed = 0.1f; fbxModelTransform->translationMin = -50.0f; fbxModelTransform->translationMax = 50.0f;

	GameObject& sprite3DObject = CreateObject("Sprite3D");
	sprite3DObject.GetTransform().translation = { 0.0f, 3.0f, 0.0f };
	SpriteRenderComponent* sprite3DRender = sprite3DObject.AddComponent<SpriteRenderComponent>(/*is3D=*/true);
	sprite3DRender->textureHandle = textures_[1].handle;
	sprite3DObject.AddComponent<TextureSelectorComponent>(sprite3DRender, &textures_, 1);

	SphereColliderComponent* sprite3DCollider = sprite3DObject.AddComponent<SphereColliderComponent>();
	sprite3DCollider->radius = 1.0f;
	sprite3DCollider->layer = CollisionLayer::kDefault;

	// 鏡（平面反射）。textureHandleは毎フレームRender()で反射テクスチャに差し替える
	GameObject& mirrorObject = CreateObject("Mirror");
	mirrorObject.GetTransform().translation = { -6.0f, 2.0f, -3.0f };
	mirrorObject.GetTransform().scale       = { 4.0f, 3.0f, 0.05f };
	CubeRenderComponent* mirrorRender = mirrorObject.AddComponent<CubeRenderComponent>();
	mirrorRender->lighting = false; // 反射像をそのまま表示するため照明計算は不要
	mirrorObject.AddComponent<MirrorComponent>(mirrorRender);

	OBBColliderComponent* mirrorCollider = mirrorObject.AddComponent<OBBColliderComponent>();
	mirrorCollider->halfSize = { 1.0f, 1.0f, 1.0f };
	mirrorCollider->layer = CollisionLayer::kEnvironment;
	mirrorCollider->isStatic = true; // Floorと同じ理由

	// Sprite2D（ピクセル座標、左上原点）。lightingデフォルトがfalseな点に注意。
	// スクリーン空間UIなのでGizmo選択対象からは外す（Objectsパネルには表示する）
	GameObject& sprite2DObject = CreateObject("Sprite2D");
	sprite2DObject.excludeFromGizmoList = true;
	sprite2DObject.GetTransform().translation = { 100.0f, 100.0f, 0.0f };
	sprite2DObject.GetTransform().scale = { 200.0f, 200.0f, 1.0f };
	SpriteRenderComponent* sprite2DRender = sprite2DObject.AddComponent<SpriteRenderComponent>(/*is3D=*/false);
	sprite2DRender->lighting = false;
	sprite2DRender->textureHandle = textures_[1].handle;
	sprite2DObject.AddComponent<TextureSelectorComponent>(sprite2DRender, &textures_, 1);

	// Sprite2Dはスクリーン座標(px)のUIなので、TransformComponentを2D表示モードにする
	TransformComponent* sprite2DTransform = sprite2DObject.GetComponent<TransformComponent>();
	sprite2DTransform->is2D = true;
	sprite2DTransform->translationSpeed = 1.0f; sprite2DTransform->translationMin = 0.0f; sprite2DTransform->translationMax = 1920.0f;
	sprite2DTransform->scaleSpeed = 1.0f; sprite2DTransform->scaleMin = 1.0f; sprite2DTransform->scaleMax = 1920.0f;

	// ライト用の空のGameObject。Transformがあるため他と同じGizmo選択に乗る
	GameObject& directionalLightObject = CreateObject("Directional Light");
	directionalLightObject.GetTransform().translation = { 0.0f, 15.0f, 0.0f };
	directionalLightObject.GetTransform().rotation = DirectionToEulerRadians({ 0.0f, -1.0f, 0.0f });
	directionalLightObject.AddComponent<DirectionalLightComponent>();

	GameObject& pointLightObject = CreateObject("Point Light");
	pointLightObject.GetTransform().translation = { 0.0f, 2.0f, 0.0f };
	pointLightObject.AddComponent<PointLightComponent>();

	GameObject& spotLightObject = CreateObject("Spot Light");
	spotLightObject.GetTransform().translation = { 2.0f, 3.0f, 0.0f };
	spotLightObject.GetTransform().rotation = DirectionToEulerRadians({ -1.0f, -1.0f, 0.0f });
	spotLightObject.AddComponent<SpotLightComponent>();

	// BGM用の空のGameObject。3D位置を持たないためGizmo選択・Objectsパネルどちらからも外す
	GameObject& bgmObject = CreateObject("BGM");
	bgmObject.excludeFromGizmoList = true;
	bgmObject.excludeFromObjectPanel = true;
	bgmObject.AddComponent<AudioSourceComponent>("Resources/Audio/BGM.mp3", "BGM", SoundType::BGM, /*loop=*/true);

	RebuildDerivedLists();
}

void PlayScene::SaveScene(const std::string& path) {
	nlohmann::json root;
	nlohmann::json objs = nlohmann::json::array();
	for (auto& obj : objects_) {
		nlohmann::json j;
		obj->ToJson(j);
		objs.push_back(j);
	}
	root["objects"] = objs;

	std::ofstream file(path);
	file << root.dump(4);
}

void PlayScene::LoadScene(const std::string& path) {
	std::ifstream file(path);
	if (!file) return;

	nlohmann::json root = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
	if (root.is_discarded() || !root.contains("objects")) return;

	// objects_を一度全部破棄してJSONの内容から丸ごと再構築する（保存時と異なる数でも復元できる）
	objects_.clear();
	ComponentLoadContext ctx{ renderer_, &textures_ };
	for (auto& objJson : root["objects"]) {
		auto obj = std::make_unique<GameObject>();
		obj->FromJson(objJson, ctx);
		objects_.push_back(std::move(obj));
	}

	RebuildDerivedLists();
	gizmoController_.ResetSelection();
}

void PlayScene::Render(float deltaTime) {
	// ImGui::NewFrame()の後、ImGui::Render()の前に呼ぶ必要がある
	ImGuizmo::BeginFrame();

	// isPlaying_中のみコンポーネント更新（Stop中はGizmoで自由に配置できるようにする）
	if (isPlaying_) {
		for (auto* obj : gizmoTargets_) obj->Update(deltaTime);
	}

	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));

	// Mirrorを探す（削除されていれば存在しない＝反射パス自体を丸ごとスキップする）
	MirrorComponent* mirror = nullptr;
	GameObject* mirrorObject = nullptr;
	for (auto& obj : objects_) {
		if (auto* m = obj->GetComponent<MirrorComponent>()) {
			mirror = m;
			mirrorObject = obj.get();
			break;
		}
	}

	if (mirror) {
		// 鏡の反射視点で、Mirror自身とSprite2D（スクリーン空間UI）以外を全部オフスクリーンへ描画する
		Collision::Plane mirrorPlane   = mirror->ComputePlane(mirrorObject->GetTransform());
		Matrix4x4        reflection    = MatrixMath::MakeReflectionMatrix(mirrorPlane);
		Matrix4x4        reflectedView = reflection * view;
		Vector3          reflectedCamPos = TransformMath::Transform(camera_->GetCameraData().position, reflection);

		renderer_->BeginMirrorPass(reflectedView, proj, reflectedCamPos);
		for (auto& obj : objects_) {
			if (obj->GetComponent<MirrorComponent>()) continue;
			if (auto* sprite = obj->GetComponent<SpriteRenderComponent>()) {
				if (!sprite->is3D) continue; // Sprite2Dは反射に映さない
			}
			if (auto* r = obj->GetComponent<RenderComponentBase>()) {
				// deltaTime=0：ModelRenderComponentのアニメーションを通常パスと二重に進めないため
				r->Draw(renderer_, obj->GetTransform(), 0.0f);
			}
		}
		renderer_->EndMirrorPass();
	}

	// ---- 通常パス ----
	renderer_->SetCamera(view, proj, camera_->GetCameraData().position);

	gizmoController_.UpdatePicking(gizmoTargets_, renderer_, view, proj);
	gizmoController_.UpdateGizmo(gizmoTargets_, renderer_, view, proj);

	for (auto& obj : objects_) {
		if (obj->GetComponent<MirrorComponent>()) continue; // Mirrorは反射テクスチャ確定後に描画する
		if (auto* r = obj->GetComponent<RenderComponentBase>()) {
			r->Draw(renderer_, obj->GetTransform(), deltaTime);
		}
	}

	if (mirror) {
		mirror->Draw(renderer_, mirrorObject->GetTransform());
	}

	colliderSystem_.ResolveAndDraw(gizmoTargets_, isPlaying_, renderer_, view, proj);

	// 各ライトコンポーネントが自分のSetter呼び出しとデバッグ表示を行う（ILightComponent経由で汎用処理）
	for (auto& obj : objects_) {
		if (auto* light = obj->GetComponent<ILightComponent>()) {
			light->SyncToRenderer(renderer_, obj->GetTransform());
			light->DrawGizmoVisualization(renderer_, obj->GetTransform(), view, proj);
		}
	}

	DrawImGui();

	// デバッグ用キー割り当て（ESCでTitle、F1でGameOverへ遷移）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = SceneType::kTitle;
	if (Input::IsTriggered(DIK_F1))     nextScene_ = SceneType::kGameOver;
}

void PlayScene::DrawGrid() {
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

void PlayScene::DrawImGui() {
	ImGui::Begin("Gizmo");

	// Stop中はGravityComponent等の更新と押し戻しを止める
	if (isPlaying_) {
		if (ImGui::Button("Stop")) isPlaying_ = false;
	} else {
		if (ImGui::Button("Play")) isPlaying_ = true;
	}
	ImGui::SameLine();
	ImGui::Text(isPlaying_ ? "(Playing)" : "(Stopped)");
	ImGui::Separator();

	// Target/Edit Collider/操作モードはGizmoControllerが描画
	gizmoController_.DrawImGui(gizmoTargets_);

	if (ImGui::Button("Delete Selected")) DeleteSelectedObject();

	ImGui::Separator();
	static const std::string kScenePath = "Resources/scene.json";
	if (ImGui::Button("Save")) SaveScene(kScenePath);
	ImGui::SameLine();
	if (ImGui::Button("Load")) LoadScene(kScenePath);

	ImGui::End();

	ImGui::Begin("Objects");

	// "Create"：選んだアーキタイプのGameObjectを1体生成してobjects_へ追加する
	static int archetypeIndex = 0;
	std::vector<const char*> archetypeNames;
	for (auto& n : kArchetypeNames) archetypeNames.push_back(n.c_str());
	ImGui::Combo("Archetype", &archetypeIndex, archetypeNames.data(), static_cast<int>(archetypeNames.size()));
	ImGui::SameLine();
	if (ImGui::Button("Create")) {
		CreateObjectFromArchetype(kArchetypeNames[archetypeIndex]);
		RebuildDerivedLists();
	}
	ImGui::Separator();

	bool firstObject = true;
	for (auto* obj : objectPanelTargets_) {
		if (!firstObject) ImGui::Separator();
		firstObject = false;
		obj->DrawImGui();
	}
	ImGui::End();

	// 共有メッシュのグローバル設定はRenderer自身が描画する
	renderer_->DrawImGui();

	AudioManager::GetInstance().DrawImGui();

	// シーン全体の設定はSceneLight自身が描画する
	renderer_->GetLight().DrawImGui();
}
