#include "PlayScene.h"
#include "../Externals/imgui/imgui.h"
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Math/VectorMath.h"
#include "../Engine/Audio/AudioManager.h"
#include "../Engine/InputDevice/InputDevice.h"
#include <cmath>
#include <algorithm>
#include <cfloat>

// 方向ベクトル → オイラー角(ラジアン、XMMatrixRotationRollPitchYaw規約)。
// 基準方向{0,0,1}から目標方向への回転をクォータニオンで求め、既存のMakeAffineMatrix/Decomposeと
// 整合する経路（回転行列→ImGuizmo::DecomposeMatrixToComponentsでのオイラー角抽出）で変換する。
// Directional/Spot Lightの初期回転を、旧SceneLightのデフォルト方向値から逆算するためだけに
// Initialize()で使う（実行時のGizmo操作パスでは不要になった。方向はTransform.rotationから
// 一方向にTransformMath::EulerRadiansToDirectionで求めるだけで済むため）
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

	// 自前の三角関数展開を書かず、既に実績のある「行列→DecomposeMatrixToComponents」の経路を再利用する
	float t[3], r[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents(&rotMat._11, t, r, s);
	return {
		XMConvertToRadians(r[0]),
		XMConvertToRadians(r[1]),
		XMConvertToRadians(r[2]) };
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

	// GameObject/コンポーネントシステムへの移行第1号。CubeRenderComponentが
	// Renderer::DrawCubeへの実際の描画呼び出しを担う。テクスチャの選択インデックスは
	// TextureSelectorComponentが自分で持ち、DrawImGuiのたびにtextureHandleへ反映する
	cubeObject_.name = "Cube";
	cubeObject_.GetTransform().translation = { -3.0f, 1.0f, 0.0f };
	CubeRenderComponent* cubeRender = cubeObject_.AddComponent<CubeRenderComponent>();
	cubeRender->textureHandle = textures_[1].handle; // 初期テクスチャ: t.png
	cubeObject_.AddComponent<TextureSelectorComponent>(cubeRender, &textures_, 1);

	// CubeはOBB（回転追従の直方体）を使う。見た目（1辺2.0相当）に近いhalfSizeを初期値にする。
	// 回転はコライダー自身では持たず、cubeObject_.GetTransform().rotationをそのまま使うため、
	// Cubeを回転させると当たり判定も一緒に追従する。"Gizmo"パネルの"Edit Collider"で
	// オフセット・サイズをドラッグ調整できる
	OBBColliderComponent* cubeCollider = cubeObject_.AddComponent<OBBColliderComponent>();
	cubeCollider->halfSize = { 1.0f, 1.0f, 1.0f };
	// レイヤー/Trigger検証用のデモ初期値：障害物想定でSolid（isTrigger=falseのまま）
	cubeCollider->layer = CollisionLayer::kObstacle;
	cubeCollider->collidesWith[static_cast<size_t>(CollisionLayer::kObstacle)] = false; // 障害物同士は判定しない
	cubeCollider->collidesWith[static_cast<size_t>(CollisionLayer::kItem)]     = false; // 障害物とアイテムは判定しない

	// 重力デモ用：CubeはSolidでEnvironment(Floor)と衝突するため、そのままGravityComponentの
	// 動作確認に使える（Gizmoで持ち上げて離すと落下しFloor上で止まる）
	cubeObject_.AddComponent<GravityComponent>();

	// 大きなFloor（Cubeを平たく大きく引き伸ばして床として使う）。CubeとFloorは
	// プロパティ構成が完全に同一なため、専用クラスを作らずCubeRenderComponentを再利用する
	floorObject_.name = "Floor";
	floorObject_.GetTransform().translation = { 0.0f, -0.5f, 0.0f };
	floorObject_.GetTransform().scale       = { 100.0f, 0.01f, 100.0f };
	CubeRenderComponent* floorRender = floorObject_.AddComponent<CubeRenderComponent>();
	floorRender->textureHandle = textures_[1].handle;
	floorObject_.AddComponent<TextureSelectorComponent>(floorRender, &textures_, 1);

	// Floorの当たり判定：X/ZはCubeと同じ基準halfSize=1（GetWorldOBBがtransform.scaleを
	// 自動で掛けるため実際の床サイズ100に一致する）。Yだけは見た目の厚み(scale.y=0.01)より
	// はるかに厚い判定用ボリュームにする：halfSize.y=100(スケール後1.0)、offset.yで見た目の
	// 上面位置(-0.49)を保ったまま下方向にだけ拡張する。GravityComponentで加速したオブジェクトが
	// 薄い床を1フレームで通り抜けてしまう「すり抜け」を防ぐための安全マージン
	OBBColliderComponent* floorCollider = floorObject_.AddComponent<OBBColliderComponent>();
	floorCollider->halfSize = { 1.0f, 100.0f, 1.0f };
	floorCollider->offset   = { 0.0f, -0.99f, 0.0f };
	floorCollider->layer = CollisionLayer::kEnvironment;
	// 背景系はisStatic=trueにして、Solidでも押し戻しで自分自身は動かないようにする
	// （isStaticの導入前はisTrigger=trueで押し戻し自体を回避していたが、本来の意図に戻す）
	floorCollider->isStatic = true;

	// Floorは巨大なオブジェクトなのでSRTのドラッグ刻み幅・範囲を広くする
	TransformComponent* floorTransform = floorObject_.GetComponent<TransformComponent>();
	floorTransform->scaleSpeed = 0.1f; floorTransform->scaleMin = 0.1f; floorTransform->scaleMax = 200.0f;
	floorTransform->translationSpeed = 0.1f; floorTransform->translationMin = -100.0f; floorTransform->translationMax = 100.0f;

	// Sphere。sphereSubdivision_はrenderer_->SetSphereSubdivision()というグローバル設定
	// （個体ごとのパラメータではない）のため、cubeSmoothness_と同様にコンポーネントには持たせない
	sphereObject_.name = "Sphere";
	sphereObject_.GetTransform().translation = { 3.0f, 1.0f, 0.0f };
	SphereRenderComponent* sphereRender = sphereObject_.AddComponent<SphereRenderComponent>();
	sphereRender->textureHandle = textures_[1].handle;
	sphereObject_.AddComponent<TextureSelectorComponent>(sphereRender, &textures_, 1);

	// Cubeとの重なり判定を確認するための検証用Collider（Collider導入検証第2号）
	SphereColliderComponent* sphereCollider = sphereObject_.AddComponent<SphereColliderComponent>();
	sphereCollider->radius = 1.0f;
	// レイヤー/Trigger検証用のデモ初期値：アイテム想定でTrigger（すり抜けて拾う）
	sphereCollider->layer = CollisionLayer::kItem;
	sphereCollider->isTrigger = true;
	sphereCollider->collidesWith[static_cast<size_t>(CollisionLayer::kObstacle)]    = false;
	sphereCollider->collidesWith[static_cast<size_t>(CollisionLayer::kItem)]        = false;
	sphereCollider->collidesWith[static_cast<size_t>(CollisionLayer::kEnvironment)] = false;

	// Triangle。triangleSmoothness_はrenderer_->SetTriangleSmoothness()というグローバル設定
	// のため、cubeSmoothness_/sphereSubdivision_と同様にコンポーネントには持たせない
	triangleObject_.name = "Triangle";
	triangleObject_.GetTransform().translation = { 0.0f, 1.0f, 0.0f };
	TriangleRenderComponent* triangleRender = triangleObject_.AddComponent<TriangleRenderComponent>();
	triangleRender->textureHandle = textures_[1].handle;
	triangleObject_.AddComponent<TextureSelectorComponent>(triangleRender, &textures_, 1);

	// Triangle用のコライダーはTriangle形状専用のものが存在しないため、Cube/Sphereと
	// 同じ「scale=1のとき半径1」基準のSphereColliderComponentで近似する
	SphereColliderComponent* triangleCollider = triangleObject_.AddComponent<SphereColliderComponent>();
	triangleCollider->radius = 1.0f;
	triangleCollider->layer = CollisionLayer::kDefault;

	// 3Dスプライト（ワールド空間）
	sprite3DObject_.name = "Sprite3D";
	sprite3DObject_.GetTransform().translation = { 0.0f, 3.0f, 0.0f };
	SpriteRenderComponent* sprite3DRender = sprite3DObject_.AddComponent<SpriteRenderComponent>(/*is3D=*/true);
	sprite3DRender->textureHandle = textures_[1].handle;
	sprite3DObject_.AddComponent<TextureSelectorComponent>(sprite3DRender, &textures_, 1);

	// Sprite3D（ワールド空間）にも他と同じ基準のSphereColliderComponentを付ける
	SphereColliderComponent* sprite3DCollider = sprite3DObject_.AddComponent<SphereColliderComponent>();
	sprite3DCollider->radius = 1.0f;
	sprite3DCollider->layer = CollisionLayer::kDefault;

	// 2DスプライトUI（ピクセル座標、左上原点）。lightingのデフォルトがfalseな点に注意
	sprite2DObject_.name = "Sprite2D";
	sprite2DObject_.GetTransform().translation = { 100.0f, 100.0f, 0.0f };
	sprite2DObject_.GetTransform().scale = { 200.0f, 200.0f, 1.0f };
	SpriteRenderComponent* sprite2DRender = sprite2DObject_.AddComponent<SpriteRenderComponent>(/*is3D=*/false);
	sprite2DRender->lighting = false;
	sprite2DRender->textureHandle = textures_[1].handle;
	sprite2DObject_.AddComponent<TextureSelectorComponent>(sprite2DRender, &textures_, 1);

	// Sprite2Dはスクリーン座標(px)のUIなので、TransformComponentを2D表示モードにする
	TransformComponent* sprite2DTransform = sprite2DObject_.GetComponent<TransformComponent>();
	sprite2DTransform->is2D = true;
	sprite2DTransform->translationSpeed = 1.0f; sprite2DTransform->translationMin = 0.0f; sprite2DTransform->translationMax = 1920.0f;
	sprite2DTransform->scaleSpeed = 1.0f; sprite2DTransform->scaleMin = 1.0f; sprite2DTransform->scaleMax = 1920.0f;

	bgm.Load("Resources/Audio/BGM.mp3");
	AudioManager::GetInstance().RegisterSound("BGM", &bgm, SoundType::BGM, true);

	modelObject_.name = "Model (OBJ)";
	modelObject_.GetTransform().translation = { 5.0f, 0.0f, 0.0f };
	// player.objはメッシュ自体の実寸がCube等の「単位1.0形状」前提と異なるため、
	// ピッキング用の基準半径を実測値に合わせて個別に設定する（scale=1なのでこの値がそのまま半径になる）
	modelObject_.pickingRadiusHint = 2.0f;
	ModelRenderComponent* modelRender = modelObject_.AddComponent<ModelRenderComponent>(
		renderer_->LoadModel("Resources/Model", "player.obj"), /*hasAnimation=*/false);
	modelRender->textureHandle = textures_[1].handle; // 初期テクスチャ: t.png
	modelObject_.AddComponent<TextureSelectorComponent>(modelRender, &textures_, 1);

	// pickingRadiusHintは既にメッシュの実測サイズに校正済みなので、そのままコライダー半径に流用する
	SphereColliderComponent* modelCollider = modelObject_.AddComponent<SphereColliderComponent>();
	modelCollider->radius = modelObject_.pickingRadiusHint;
	modelCollider->layer = CollisionLayer::kDefault;

	// Assimp導入確認用（FBX読み込みテスト。ボーン+アニメーション付きのHumanModel_ver2.fbxで確認）
	fbxModelObject_.name = "FBX Model";
	fbxModelObject_.GetTransform().translation = { 8.0f, 0.0f, 0.0f };
	// MixamoモデルはFBXのUnitScaleFactorメタデータが1.0のまま実寸(cm相当)で出力されており、
	// 他オブジェクトと同じ単位系に合わせるには実測で0.01倍が丁度良かった
	fbxModelObject_.GetTransform().scale = { 0.01f, 0.01f, 0.01f };
	// ピッキング半径 = pickingRadiusHint * scale なので、実寸(cm相当)の人間サイズに対して
	// scale=0.01を掛けた後に半径1.0m程度になるよう、逆算して100.0を基準値にする
	fbxModelObject_.pickingRadiusHint = 100.0f;
	ModelRenderComponent* fbxModelRender = fbxModelObject_.AddComponent<ModelRenderComponent>(
		renderer_->LoadModel("Resources/Model", "HumanModel_ver2.fbx"), /*hasAnimation=*/true);
	fbxModelRender->textureHandle = textures_[0].handle; // 現状未選択(なし)のまま据え置き
	fbxModelObject_.AddComponent<TextureSelectorComponent>(fbxModelRender, &textures_, 0);

	// SphereColliderComponentのradiusはtransform.scaleの影響を受けない絶対値のため、
	// ピッキングと同じ換算（pickingRadiusHint * scale）をあらかじめ計算しておく
	SphereColliderComponent* fbxModelCollider = fbxModelObject_.AddComponent<SphereColliderComponent>();
	fbxModelCollider->radius = fbxModelObject_.pickingRadiusHint * fbxModelObject_.GetTransform().scale.x;
	fbxModelCollider->layer = CollisionLayer::kDefault;

	// FBX Modelは実寸(cm相当)にscale=0.01を掛けている関係でscaleの刻み幅を細かくする
	TransformComponent* fbxModelTransform = fbxModelObject_.GetComponent<TransformComponent>();
	fbxModelTransform->scaleSpeed = 0.001f; fbxModelTransform->scaleMin = 0.001f; fbxModelTransform->scaleMax = 1.0f;
	fbxModelTransform->translationSpeed = 0.1f; fbxModelTransform->translationMin = -50.0f; fbxModelTransform->translationMax = 50.0f;

	// 鏡（平面反射）。floor同様に薄く引き伸ばしたCubeを板として使う。textureHandleは
	// 毎フレームRender()内でRenderer::GetMirrorTextureHandle()に差し替えるためここでは未設定
	mirrorObject_.name = "Mirror";
	mirrorObject_.GetTransform().translation = { -6.0f, 2.0f, -3.0f };
	mirrorObject_.GetTransform().scale       = { 4.0f, 3.0f, 0.05f };
	CubeRenderComponent* mirrorRender = mirrorObject_.AddComponent<CubeRenderComponent>();
	mirrorRender->lighting = false; // 反射像をそのまま表示するため照明計算は不要

	// Mirrorの当たり判定：Floorと同じ理由でhalfSize={1,1,1}＋isTrigger=true（背景系は押し戻さない）
	OBBColliderComponent* mirrorCollider = mirrorObject_.AddComponent<OBBColliderComponent>();
	mirrorCollider->halfSize = { 1.0f, 1.0f, 1.0f };
	mirrorCollider->layer = CollisionLayer::kEnvironment;
	// Floorと同じ理由でisStatic=trueにする
	mirrorCollider->isStatic = true;

	// ライト用の「空のGameObject」。Render/Colliderは持たず、対応するXxxLightComponentのみを
	// 持つ。Transformが実在するため、他のオブジェクトと全く同じGizmo選択・ドラッグ編集の
	// 仕組みに乗る（旧GizmoTarget::kPointLight/kSpotLightのような特殊分岐が不要になる）。
	// 初期値は旧SceneLight::LightDataのデフォルト値をそのまま踏襲し、見た目が変わらないようにする
	directionalLightObject_.name = "Directional Light";
	directionalLightObject_.GetTransform().translation = { 0.0f, 15.0f, 0.0f }; // 旧可視化位置(-direction*15)を踏襲
	directionalLightObject_.GetTransform().rotation = DirectionToEulerRadians({ 0.0f, -1.0f, 0.0f }); // 旧directionデフォルト
	directionalLightObject_.AddComponent<DirectionalLightComponent>();

	pointLightObject_.name = "Point Light";
	pointLightObject_.GetTransform().translation = { 0.0f, 2.0f, 0.0f }; // 旧pointPositionデフォルト
	pointLightObject_.AddComponent<PointLightComponent>();

	spotLightObject_.name = "Spot Light";
	spotLightObject_.GetTransform().translation = { 2.0f, 3.0f, 0.0f }; // 旧spotPositionデフォルト
	spotLightObject_.GetTransform().rotation = DirectionToEulerRadians({ -1.0f, -1.0f, 0.0f }); // 旧spotDirectionデフォルト
	spotLightObject_.AddComponent<SpotLightComponent>();

	// 全ての通常オブジェクトのGameObjectをギズモ選択対象として登録する。
	// name(GameObject::name)がそのままImGuiコンボの表示名になる。
	// sprite2DObject_は既存通りギズモ対象外のため含めない
	gizmoTargets_ = {
		&cubeObject_, &sphereObject_, &triangleObject_, &floorObject_,
		&sprite3DObject_, &modelObject_, &fbxModelObject_, &mirrorObject_,
		&directionalLightObject_, &pointLightObject_, &spotLightObject_,
	};

	// "Objects"パネルに表示する全GameObjectの一覧（表示順そのまま）。gizmoTargets_と違い
	// sprite2DObject_（スクリーン空間UI）も含む
	objectPanelTargets_ = {
		&floorObject_, &triangleObject_, &cubeObject_, &sphereObject_,
		&modelObject_, &fbxModelObject_, &sprite3DObject_, &mirrorObject_,
		&sprite2DObject_, &directionalLightObject_, &pointLightObject_, &spotLightObject_,
	};
}

Collision::Plane PlayScene::ComputeMirrorPlane(const Transform& mirrorTransform) const {
	// 鏡面オブジェクトの前面（Cube.cppのZ+面）をローカル法線として使う。
	// Camera::HandleInputと同じ「回転のみのアフィン行列で方向ベクトルを変換する」パターン
	Matrix4x4 rotationOnly = TransformMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, mirrorTransform.rotation, { 0.0f, 0.0f, 0.0f });
	Vector3 normal = TransformMath::Transform({ 0.0f, 0.0f, 1.0f }, rotationOnly);
	float distance = VectorMath::Dot(normal, mirrorTransform.translation);
	return Collision::Plane{ normal, distance };
}

void PlayScene::Render(float deltaTime) {
	// ImGuizmo導入確認用（Step0）。ImGui::NewFrame()の後、ImGui::Render()の前に呼ぶ必要がある
	ImGuizmo::BeginFrame();

	// 全オブジェクトのコンポーネントを更新する（GravityComponent等、毎フレームTransformを
	// 書き換えるコンポーネントはここで動く）。この直後のResolveAndDrawColliderGizmosが
	// 同じフレーム内で地面との重なりを検出して押し戻すため、「落下→着地」が1フレームで揃う。
	// Stop中（isPlaying_=false）はGizmoで自由に配置できるよう、シミュレーション自体を止める
	if (isPlaying_) {
		for (auto* obj : gizmoTargets_) obj->Update(deltaTime);
	}

	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));

	// ---- 鏡（反射）パス：鏡の平面に対して反射させた視点で、床・Cube・Sphere・Triangleを
	// オフスクリーンテクスチャへ描く。mirrorObject_自身は描かない（無限反射を避けるため） ----
	Collision::Plane mirrorPlane   = ComputeMirrorPlane(mirrorObject_.GetTransform());
	Matrix4x4        reflection    = MatrixMath::MakeReflectionMatrix(mirrorPlane);
	Matrix4x4        reflectedView = reflection * view;
	Vector3           reflectedCamPos = TransformMath::Transform(camera_->GetCameraData().position, reflection);

	renderer_->BeginMirrorPass(reflectedView, proj, reflectedCamPos);
	floorObject_.GetComponent<CubeRenderComponent>()->Draw(renderer_, floorObject_.GetTransform());
	triangleObject_.GetComponent<TriangleRenderComponent>()->Draw(renderer_, triangleObject_.GetTransform());
	cubeObject_.GetComponent<CubeRenderComponent>()->Draw(renderer_, cubeObject_.GetTransform());
	sphereObject_.GetComponent<SphereRenderComponent>()->Draw(renderer_, sphereObject_.GetTransform());
	renderer_->EndMirrorPass();

	// ---- 通常パス ----
	renderer_->SetCamera(view, proj, camera_->GetCameraData().position);

	floorObject_.GetComponent<CubeRenderComponent>()->Draw(renderer_, floorObject_.GetTransform());

	// マウスピッキング：3Dビュー上の左クリックでギズモ選択対象を切り替える（コンボボックス選択と共存）
	UpdatePicking(view, proj);

	// Blenderライクなギズモ操作："Gizmo"パネルで選んだ1オブジェクトのTransformをドラッグで編集する
	UpdateGizmo(view, proj);

	triangleObject_.GetComponent<TriangleRenderComponent>()->Draw(renderer_, triangleObject_.GetTransform());
	cubeObject_.GetComponent<CubeRenderComponent>()->Draw(renderer_, cubeObject_.GetTransform());
	sphereObject_.GetComponent<SphereRenderComponent>()->Draw(renderer_, sphereObject_.GetTransform());
	modelObject_.GetComponent<ModelRenderComponent>()->Draw(renderer_, modelObject_.GetTransform(), deltaTime);
	fbxModelObject_.GetComponent<ModelRenderComponent>()->Draw(renderer_, fbxModelObject_.GetTransform(), deltaTime);
	sprite3DObject_.GetComponent<SpriteRenderComponent>()->Draw(renderer_, sprite3DObject_.GetTransform());
	sprite2DObject_.GetComponent<SpriteRenderComponent>()->Draw(renderer_, sprite2DObject_.GetTransform());

	// 鏡面オブジェクト自体を、反射パスで描いたテクスチャを貼って表示する
	CubeRenderComponent* mirrorRender = mirrorObject_.GetComponent<CubeRenderComponent>();
	mirrorRender->textureHandle = renderer_->GetMirrorTextureHandle();
	mirrorRender->Draw(renderer_, mirrorObject_.GetTransform());

	// Collider（当たり判定）の判定・押し戻し・可視化（詳細はResolveAndDrawColliderGizmos参照）
	ResolveAndDrawColliderGizmos(view, proj);

	// 光源をSceneLightへ反映し、可視化する（各コンポーネントが自分のSetter呼び出しと
	// デバッグ表示を担う。位置・向きはライトオブジェクトのTransformから導出される）
	if (auto* c = directionalLightObject_.GetComponent<DirectionalLightComponent>()) {
		c->SyncToRenderer(renderer_, directionalLightObject_.GetTransform());
		c->DrawGizmoVisualization(renderer_, directionalLightObject_.GetTransform(), view, proj);
	}
	if (auto* c = pointLightObject_.GetComponent<PointLightComponent>()) {
		c->SyncToRenderer(renderer_, pointLightObject_.GetTransform());
		c->DrawGizmoVisualization(renderer_, pointLightObject_.GetTransform(), view, proj);
	}
	if (auto* c = spotLightObject_.GetComponent<SpotLightComponent>()) {
		c->SyncToRenderer(renderer_, spotLightObject_.GetTransform());
		c->DrawGizmoVisualization(renderer_, spotLightObject_.GetTransform(), view, proj);
	}

	//DrawGrid();
	DrawImGui();

	// 仮の動作確認用ショートカット（本来はPlayer-Obstacleの当たり判定等からゲームオーバーへ
	// 遷移する想定。実際のPlayer/Obstacleが実装されるまでの暫定的なデバッグ用のキー割り当て）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = SceneType::kTitle;
	if (Input::IsTriggered(DIK_F1))     nextScene_ = SceneType::kGameOver;
}

Transform* PlayScene::GetGizmoTargetTransform() {
	if (gizmoTargetIndex_ >= 0 && gizmoTargetIndex_ < static_cast<int>(gizmoTargets_.size())) {
		GameObject* obj = gizmoTargets_[gizmoTargetIndex_];
		if (editCollider_) {
			// Collider編集モード：SphereCollider/OBBColliderのどちらか一方を対象にする
			// （両方持つ場合はSphereを優先。今回は1オブジェクト1コライダー運用を想定）
			if (obj->GetComponent<SphereColliderComponent>() || obj->GetComponent<OBBColliderComponent>()) {
				return &colliderGizmoScratch_;
			}
			return nullptr; // Colliderを持たないオブジェクトを選んでいる場合は何も編集しない
		}
		return &obj->GetTransform();
	}
	return nullptr;
}

void PlayScene::ResolveAndDrawColliderGizmos(const Matrix4x4& view, const Matrix4x4& proj) {
	constexpr Vector4 kNoOverlapColor      = { 0.2f, 1.0f, 0.2f, 1.0f }; // 緑：重なりなし
	constexpr Vector4 kTriggerOverlapColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄：Trigger込みの重なり
	constexpr Vector4 kSolidOverlapColor   = { 1.0f, 0.2f, 0.2f, 1.0f }; // 赤：Solid同士の重なり

	// 全オブジェクトのColliderを先に集めておく（N×Nの重なり判定を1回で済ませるため）
	struct Entry {
		GameObject* obj;
		SphereColliderComponent* sphere;
		OBBColliderComponent* obb;
		ColliderComponentBase* base; // layer/isTrigger参照用。sphere/obbのどちらか一方を指す
		bool overlappingSolid   = false;
		bool overlappingTrigger = false;
	};
	std::vector<Entry> entries;
	for (auto* obj : gizmoTargets_) {
		auto* sphere = obj->GetComponent<SphereColliderComponent>();
		auto* obb    = obj->GetComponent<OBBColliderComponent>();
		ColliderComponentBase* base = sphere
			? static_cast<ColliderComponentBase*>(sphere)
			: static_cast<ColliderComponentBase*>(obb);
		if (base) entries.push_back({ obj, sphere, obb, base });
	}

	// 総当たりで重なりを判定（オブジェクト数が少ないため計算量は無視できる）
	for (size_t i = 0; i < entries.size(); i++) {
		for (size_t j = i + 1; j < entries.size(); j++) {
			auto& a = entries[i];
			auto& b = entries[j];

			// レイヤーの組み合わせが判定対象外なら幾何計算自体をスキップする
			// （「Obstacle同士は判定しない」等をここ1行で表現できる）
			if (!ShouldLayersCollide(*a.base, *b.base)) continue;

			bool hit = false;
			if (a.sphere && b.sphere) {
				hit = Collision::SphereSphere(a.sphere->GetWorldSphere(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()));
			} else if (a.obb && b.obb) {
				hit = Collision::OBBOBB(a.obb->GetWorldOBB(a.obj->GetTransform()), b.obb->GetWorldOBB(b.obj->GetTransform()));
			} else if (a.sphere && b.obb) {
				hit = Collision::OBBSphere(b.obb->GetWorldOBB(b.obj->GetTransform()), a.sphere->GetWorldSphere(a.obj->GetTransform()));
			} else if (a.obb && b.sphere) {
				hit = Collision::OBBSphere(a.obb->GetWorldOBB(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()));
			}
			if (!hit) continue;

			// 片方でもTriggerならこのペアはTrigger重なり、両方Solidの場合のみSolid重なり
			if (a.base->isTrigger || b.base->isTrigger) {
				a.overlappingTrigger = true; b.overlappingTrigger = true;
			} else {
				a.overlappingSolid = true; b.overlappingSolid = true;

				// Stop中（isPlaying_=false）は押し戻しを行わない。重なりの検知・色分け表示だけは
				// 常時行い、Gizmoでの自由な配置を妨げないようにする
				if (!isPlaying_) continue;

				// 両方Solid：実際に押し戻して重なりを解消する（半分ずつ均等に分配）
				Vector3 normal{ 0.0f, 0.0f, 0.0f }; // a→b向き
				float   depth = 0.0f;
				bool    resolved = false;
				if (a.sphere && b.sphere) {
					resolved = Collision::SphereSpherePenetration(a.sphere->GetWorldSphere(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()), normal, depth);
				} else if (a.obb && b.obb) {
					resolved = Collision::OBBOBBPenetration(a.obb->GetWorldOBB(a.obj->GetTransform()), b.obb->GetWorldOBB(b.obj->GetTransform()), normal, depth);
				} else if (a.sphere && b.obb) {
					// OBBSpherePenetrationはobb→sphere向きを返すため、a(sphere)→b(obb)向きに揃えるため反転する
					resolved = Collision::OBBSpherePenetration(b.obb->GetWorldOBB(b.obj->GetTransform()), a.sphere->GetWorldSphere(a.obj->GetTransform()), normal, depth);
					normal = normal * -1.0f;
				} else if (a.obb && b.sphere) {
					resolved = Collision::OBBSpherePenetration(a.obb->GetWorldOBB(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()), normal, depth);
				}
				if (resolved) {
					// isStaticな側は押し戻しで動かさない。両方staticなら押し戻し自体をスキップし、
					// 片方だけstaticならもう片方に100%を割り当てる（デフォルトは従来通り50/50）
					float aRatio = 0.5f, bRatio = 0.5f;
					if (a.base->isStatic && !b.base->isStatic) {
						aRatio = 0.0f; bRatio = 1.0f;
					} else if (b.base->isStatic && !a.base->isStatic) {
						aRatio = 1.0f; bRatio = 0.0f;
					} else if (a.base->isStatic && b.base->isStatic) {
						aRatio = 0.0f; bRatio = 0.0f;
					}

					Vector3 aDelta = normal * (-depth * aRatio);
					Vector3 bDelta = normal * (depth * bRatio);
					a.obj->GetTransform().translation = a.obj->GetTransform().translation + aDelta;
					b.obj->GetTransform().translation = b.obj->GetTransform().translation + bDelta;

					// 重力で落下中のオブジェクトが押し戻しで上方向に着地した場合、蓄積された落下速度を
					// リセットする（リセットしないと同じ速度で再びめり込み、押し戻しが毎フレーム
					// 発生して振動してしまう）
					if (GravityComponent* aGravity = a.obj->GetComponent<GravityComponent>()) {
						if (aDelta.y > 0.0f && aGravity->velocityY < 0.0f) aGravity->velocityY = 0.0f;
					}
					if (GravityComponent* bGravity = b.obj->GetComponent<GravityComponent>()) {
						if (bDelta.y > 0.0f && bGravity->velocityY < 0.0f) bGravity->velocityY = 0.0f;
					}
				}
			}
		}
	}

	// 描画は各コンポーネント自身のDrawWireframeに委譲する（PlaySceneは判定結果の色だけ渡す）。
	// 1つのオブジェクトが複数と同時に重なる場合はSolid重なりを優先して赤にする
	// （「ぶつかって止まる」方が「すり抜けて拾う」より目視上の優先度が高いため）
	for (auto& e : entries) {
		Vector4 color = e.overlappingSolid   ? kSolidOverlapColor
		              : e.overlappingTrigger ? kTriggerOverlapColor
		                                     : kNoOverlapColor;
		if (e.sphere) e.sphere->DrawWireframe(renderer_, e.obj->GetTransform(), color, view, proj);
		if (e.obb)    e.obb->DrawWireframe(renderer_, e.obj->GetTransform(), color, view, proj);
	}
}


void PlayScene::UpdatePicking(const Matrix4x4& view, const Matrix4x4& proj) {
	bool leftPressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool triggered = leftPressed && !prevMouseLeftPressed_;
	prevMouseLeftPressed_ = leftPressed;

	if (!triggered) return;
	if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return; // ギズモ操作中/ホバー中は発火しない
	if (ImGui::GetIO().WantCaptureMouse) return;           // ImGuiパネル上のクリックは無視

	// スクリーン座標 → NDC → ワールド空間レイ
	ImVec2 mousePos = ImGui::GetMousePos();
	float width  = static_cast<float>(renderer_->GetClientWidth());
	float height = static_cast<float>(renderer_->GetClientHeight());
	float ndcX = (mousePos.x / width)  * 2.0f - 1.0f;
	float ndcY = 1.0f - (mousePos.y / height) * 2.0f;

	Matrix4x4 invViewProj = MatrixMath::Inverse(view * proj);
	Vector3 nearPoint = TransformMath::Transform({ ndcX, ndcY, 0.0f }, invViewProj);
	Vector3 farPoint  = TransformMath::Transform({ ndcX, ndcY, 1.0f }, invViewProj);

	Collision::Ray ray;
	ray.origin = nearPoint;
	ray.diff   = farPoint - nearPoint;

	// gizmoTargets_内の全オブジェクトをBounding Sphere（pickingRadiusHint * scaleの最大成分を
	// 半径とする）とみなし、最もt値が小さい（＝最も手前の）ものを選ぶ。
	// pickingRadiusHintはGameObjectごとの基準半径（scale=1のときの半径）で、Cube/Sphere/
	// Triangleは1.0のデフォルトのままでよいが、Model系はメッシュ実寸とscaleの対応が個体ごとに
	// 違うためInitialize()で個別調整している。
	// Floorはscale={100, 0.01, 100}のような極端に平たい形状で、scale最大成分を半径にすると
	// 実際の見た目よりはるかに巨大な球になり、どこをクリックしても最優先でヒットしてしまうため、
	// ピッキング判定からだけ除外する（コンボボックスからの選択は引き続き可能）
	int   closestIndex = -1;
	float closestT = FLT_MAX;
	for (int i = 0; i < static_cast<int>(gizmoTargets_.size()); i++) {
		if (gizmoTargets_[i] == &floorObject_) continue;
		const Transform& t = gizmoTargets_[i]->GetTransform();
		float maxScale = (std::max)({ t.scale.x, t.scale.y, t.scale.z });
		float radius = gizmoTargets_[i]->pickingRadiusHint * maxScale;
		Collision::Sphere sphere{ t.translation, radius };
		float hitT;
		if (Collision::RaySphere(ray, sphere, hitT) && hitT < closestT) {
			closestT = hitT;
			closestIndex = i;
		}
	}

	if (closestIndex >= 0) {
		gizmoTargetIndex_ = closestIndex;
	}
	// 何にも当たらなかった場合は現在の選択状態を維持する
}

void PlayScene::UpdateGizmo(const Matrix4x4& view, const Matrix4x4& proj) {
	Transform* target = GetGizmoTargetTransform();
	if (!target) return;

	// Collider編集モードの場合、操作開始前に現在のoffset/radius(またはhalfSize)を
	// colliderGizmoScratch_へ反映する（ドラッグ中は再計算しない、上記ライトと同じ理由）
	GameObject* gizmoTargetObj = (gizmoTargetIndex_ >= 0 && gizmoTargetIndex_ < static_cast<int>(gizmoTargets_.size()))
		? gizmoTargets_[gizmoTargetIndex_] : nullptr;
	if (editCollider_ && gizmoTargetObj && !ImGuizmo::IsUsing()) {
		if (auto* sphereCol = gizmoTargetObj->GetComponent<SphereColliderComponent>()) {
			colliderGizmoScratch_.translation = gizmoTargetObj->GetTransform().translation + sphereCol->offset;
			colliderGizmoScratch_.scale       = { sphereCol->radius, sphereCol->radius, sphereCol->radius };
		} else if (auto* obbCol = gizmoTargetObj->GetComponent<OBBColliderComponent>()) {
			colliderGizmoScratch_.translation = gizmoTargetObj->GetTransform().translation + obbCol->offset;
			colliderGizmoScratch_.scale       = { obbCol->halfSize.x * 2.0f, obbCol->halfSize.y * 2.0f, obbCol->halfSize.z * 2.0f };
		}
	}

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(0, 0, (float)renderer_->GetClientWidth(), (float)renderer_->GetClientHeight());

	Matrix4x4 world = TransformMath::MakeAffineMatrix(target->scale, target->rotation, target->translation);

	ImGuizmo::OPERATION operation = gizmoOperation_;
	// Collider編集中はRotate操作を無効化する（Sphere/AABBに回転の意味がないため）
	if (editCollider_ && operation == ImGuizmo::ROTATE) {
		operation = ImGuizmo::TRANSLATE;
	}

	if (ImGuizmo::Manipulate(&view._11, &proj._11, operation, ImGuizmo::WORLD, &world._11)) {
		float t[3], r[3], s[3];
		ImGuizmo::DecomposeMatrixToComponents(&world._11, t, r, s);
		target->translation = { t[0], t[1], t[2] };
		target->rotation    = {
			DirectX::XMConvertToRadians(r[0]),
			DirectX::XMConvertToRadians(r[1]),
			DirectX::XMConvertToRadians(r[2]) }; // ImGuizmoは度数法、Transform.rotationはラジアン
		target->scale        = { s[0], s[1], s[2] };

		// Collider編集中の場合、ワールド座標系のtranslation/scaleをoffset/radius(またはhalfSize)へ変換して書き戻す
		if (editCollider_ && gizmoTargetObj) {
			if (auto* sphereCol = gizmoTargetObj->GetComponent<SphereColliderComponent>()) {
				sphereCol->offset = target->translation - gizmoTargetObj->GetTransform().translation;
				sphereCol->radius = target->scale.x; // Scaleギズモは等方的なドラッグを想定し、xの値を採用
			} else if (auto* obbCol = gizmoTargetObj->GetComponent<OBBColliderComponent>()) {
				obbCol->offset   = target->translation - gizmoTargetObj->GetTransform().translation;
				obbCol->halfSize = { target->scale.x * 0.5f, target->scale.y * 0.5f, target->scale.z * 0.5f };
			}
		}
	}
}

void PlayScene::DrawGrid() {
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

void PlayScene::DrawImGui() {

	ImGui::Begin("Gizmo");

	// Unityの Play/Stop に相当するボタン。Stop中（isPlaying_=false）はGravityComponent等の
	// シミュレーションと当たり判定の押し戻しを止め、Gizmoで自由にオブジェクトを配置できる
	if (isPlaying_) {
		if (ImGui::Button("Stop")) isPlaying_ = false;
	} else {
		if (ImGui::Button("Play")) isPlaying_ = true;
	}
	ImGui::SameLine();
	ImGui::Text(isPlaying_ ? "(Playing)" : "(Stopped)");
	ImGui::Separator();

	// コンボの選択肢：0="None", 1..N=gizmoTargets_[0..N-1]の名前（ライト用GameObjectも
	// 他と同じくTransformを持つため、ここに同列で並ぶ）
	std::vector<const char*> comboNames;
	comboNames.push_back("None");
	for (auto* obj : gizmoTargets_) comboNames.push_back(obj->name.c_str());

	int currentCombo = gizmoTargetIndex_ + 1; // -1("None")+1=0, 0以上はそのまま+1

	if (ImGui::Combo("Target", &currentCombo, comboNames.data(), static_cast<int>(comboNames.size()))) {
		gizmoTargetIndex_ = currentCombo - 1; // 0("None")-1=-1
	}

	// Collider編集モード：選択中オブジェクトがCollider（Sphere/Box）を持つ場合のみ有効化できる。
	// オンの間、ギズモの対象はGameObject本体のTransformではなくColliderのオフセット/サイズになる
	bool hasCollider = false;
	if (gizmoTargetIndex_ >= 0 && gizmoTargetIndex_ < static_cast<int>(gizmoTargets_.size())) {
		GameObject* obj = gizmoTargets_[gizmoTargetIndex_];
		hasCollider = (obj->GetComponent<SphereColliderComponent>() != nullptr) ||
		              (obj->GetComponent<OBBColliderComponent>() != nullptr);
	}
	if (!hasCollider) editCollider_ = false; // Colliderを持たないオブジェクト選択中は強制オフ
	if (!hasCollider) ImGui::BeginDisabled();
	ImGui::Checkbox("Edit Collider", &editCollider_);
	if (!hasCollider) ImGui::EndDisabled();

	// Collider編集中はRotateに意味がないためグレーアウトする（Sphere/AABBに回転の概念がない）
	bool disableRotate = editCollider_;

	if (ImGui::RadioButton("Translate", gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (disableRotate) ImGui::BeginDisabled();
	if (ImGui::RadioButton("Rotate", gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
	if (disableRotate) ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;

	// Collider同士の重なりは3Dビュー上のワイヤーフレーム色（緑=重なりなし/黄=Trigger込みの
	// 重なり/赤=Solid同士の重なり）で表示されるため、テキストでの重複表示はしない。
	// なお赤（Solid同士）の場合は検知だけでなく実際に押し戻しも行われる（ResolveAndDrawColliderGizmos()参照）
	ImGui::End();

	ImGui::Begin("Objects");
	bool firstObject = true;
	for (auto* obj : objectPanelTargets_) {
		if (!firstObject) ImGui::Separator();
		firstObject = false;

		// Cube/Triangle SmoothnessとSphere Subdivisionは個体ごとのパラメータではなく、
		// Renderer側で1つだけ共有しているメッシュ全体に効くグローバル設定（cube_/triangle_/sphere_は
		// それぞれ単一インスタンス）のため、コンポーネント化せずここで個別に挿し込む
		if (obj == &cubeObject_) {
			if (ImGui::SliderFloat("Cube Smoothness", &cubeSmoothness_, 0.0f, 1.0f))
				renderer_->SetCubeSmoothness(cubeSmoothness_);
		} else if (obj == &triangleObject_) {
			if (ImGui::SliderFloat("Triangle Smoothness", &triangleSmoothness_, 0.0f, 1.0f))
				renderer_->SetTriangleSmoothness(triangleSmoothness_);
		} else if (obj == &sphereObject_) {
			if (ImGui::SliderInt("Sphere Subdivision", &sphereSubdivision_, 1, static_cast<int>(Renderer::kSphereMaxSubdivision)))
				renderer_->SetSphereSubdivision(static_cast<uint32_t>(sphereSubdivision_));
		}

		obj->DrawImGui(); // 名前見出し＋Transform/Render/Texture/Collider/Gravity/Lightの項目を自動描画する
	}

	cubeObject_.DrawImGui();
	ImGui::End();

	AudioManager::GetInstance().DrawImGui();

	auto& light = renderer_->GetLight();
	auto& data = light.GetData();

	ImGui::Begin("Lighting");

	ImGui::Text("Toon Shading");
	bool enableToon = data.enableToon != 0;
	if (ImGui::Checkbox("Enable Toon", &enableToon)) {
		light.SetEnableToon(enableToon);
	}
	if (ImGui::SliderFloat("Toon Threshold", &data.toonThreshold, 0.0f, 1.0f)) {
		light.SetToonThreshold(data.toonThreshold);
	}

	ImGui::Separator();
	ImGui::Text("Specular (Blinn-Phong)");
	bool enableSpecular = data.enableSpecular != 0;
	if (ImGui::Checkbox("Enable Specular", &enableSpecular)) {
		light.SetEnableSpecular(enableSpecular);
	}
	if (ImGui::ColorEdit3("Specular Color", &data.specularColor.x)) {
		light.SetSpecularColor(data.specularColor);
	}
	if (ImGui::SliderFloat("Shininess", &data.shininess, 1.0f, 200.0f)) {
		light.SetShininess(data.shininess);
	}

	ImGui::Separator();
	ImGui::Text("Rim Light");
	bool enableRim = data.enableRim != 0;
	if (ImGui::Checkbox("Enable Rim", &enableRim)) {
		light.SetEnableRim(enableRim);
	}
	if (ImGui::ColorEdit3("Rim Color", &data.rimColor.x)) {
		light.SetRimColor(data.rimColor);
	}
	if (ImGui::SliderFloat("Rim Power", &data.rimPower, 0.1f, 8.0f)) {
		light.SetRimPower(data.rimPower);
	}
	if (ImGui::SliderFloat("Rim Strength", &data.rimStrength, 0.0f, 4.0f)) {
		light.SetRimStrength(data.rimStrength);
	}

	ImGui::End();

}
