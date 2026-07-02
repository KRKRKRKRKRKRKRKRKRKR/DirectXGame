#include "Game.h"
#include "../Externals/imgui/imgui.h"
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Engine/Audio/AudioManager.h"
#include <cmath>
#include <algorithm>
#include <cfloat>

void Game::RebuildGridCubes() {
	constexpr float kSpacing  = 2.0f;
	constexpr float kBaseY    = 0.5f; // 床(floor_のY≈-0.5)にめり込まないよう最下段を底上げする
	const float     kOffsetXZ = (gridSize_ - 1) * kSpacing / 2.0f;

	gridCubes_.clear();
	gridCubes_.reserve(static_cast<size_t>(gridSize_) * gridSize_ * gridSize_);
	for (int y = 0; y < gridSize_; y++) {
		for (int z = 0; z < gridSize_; z++) {
			for (int x = 0; x < gridSize_; x++) {
				Transform t;
				t.translation = {
					x * kSpacing - kOffsetXZ,
					y * kSpacing + kBaseY,
					z * kSpacing - kOffsetXZ
				};
				t.scale = { 1.0f, 1.0f, 1.0f };
				gridCubes_.push_back(t);
			}
		}
	}
}

void Game::Initialize(Renderer* renderer, Camera* camera) {
	renderer_ = renderer;
	camera_ = camera;

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

	sprite2DTexIndex_ = 1; // t.png をデフォルト
	sprite3DTexIndex_ = 1;
	triangleTexIndex_ = 1;
	cubeTexIndex_ = 1;
	sphereTexIndex_ = 1;

	// GameObject/コンポーネントシステムへの移行第1号。CubeRenderComponentが
	// Renderer::DrawCubeへの実際の描画呼び出しを担う（cubeTexIndex_はtextures_配列への
	// UI選択インデックスとしてGame側に残し、選んだ結果のhandleだけコンポーネントへ渡す）
	cubeObject_.name = "Cube";
	cubeObject_.transform.translation = { -3.0f, 1.0f, 0.0f };
	CubeRenderComponent* cubeRender = cubeObject_.AddComponent<CubeRenderComponent>();
	cubeRender->textureHandle = textures_[cubeTexIndex_].handle;

	// CubeはOBB（回転追従の直方体）を使う。見た目（1辺2.0相当）に近いhalfSizeを初期値にする。
	// 回転はコライダー自身では持たず、cubeObject_.transform.rotationをそのまま使うため、
	// Cubeを回転させると当たり判定も一緒に追従する。"Gizmo"パネルの"Edit Collider"で
	// オフセット・サイズをドラッグ調整できる
	OBBColliderComponent* cubeCollider = cubeObject_.AddComponent<OBBColliderComponent>();
	cubeCollider->halfSize = { 1.0f, 1.0f, 1.0f };

	// 大きなFloor（Cubeを平たく大きく引き伸ばして床として使う）。CubeとFloorは
	// プロパティ構成が完全に同一なため、専用クラスを作らずCubeRenderComponentを再利用する
	floorTexIndex_ = 1;
	floorObject_.name = "Floor";
	floorObject_.transform.translation = { 0.0f, -0.5f, 0.0f };
	floorObject_.transform.scale       = { 100.0f, 0.01f, 100.0f };
	CubeRenderComponent* floorRender = floorObject_.AddComponent<CubeRenderComponent>();
	floorRender->textureHandle = textures_[floorTexIndex_].handle;

	// Sphere。sphereSubdivision_はrenderer_->SetSphereSubdivision()というグローバル設定
	// （個体ごとのパラメータではない）のため、cubeSmoothness_と同様にコンポーネントには持たせない
	sphereObject_.name = "Sphere";
	sphereObject_.transform.translation = { 3.0f, 1.0f, 0.0f };
	SphereRenderComponent* sphereRender = sphereObject_.AddComponent<SphereRenderComponent>();
	sphereRender->textureHandle = textures_[sphereTexIndex_].handle;

	// Cubeとの重なり判定を確認するための検証用Collider（Collider導入検証第2号）
	SphereColliderComponent* sphereCollider = sphereObject_.AddComponent<SphereColliderComponent>();
	sphereCollider->radius = 1.0f;

	// Triangle。triangleSmoothness_はrenderer_->SetTriangleSmoothness()というグローバル設定
	// のため、cubeSmoothness_/sphereSubdivision_と同様にコンポーネントには持たせない
	triangleObject_.name = "Triangle";
	triangleObject_.transform.translation = { 0.0f, 1.0f, 0.0f };
	TriangleRenderComponent* triangleRender = triangleObject_.AddComponent<TriangleRenderComponent>();
	triangleRender->textureHandle = textures_[triangleTexIndex_].handle;

	RebuildGridCubes();

	// 3Dスプライト（ワールド空間）
	sprite3DObject_.name = "Sprite3D";
	sprite3DObject_.transform.translation = { 0.0f, 3.0f, 0.0f };
	SpriteRenderComponent* sprite3DRender = sprite3DObject_.AddComponent<SpriteRenderComponent>(/*is3D=*/true);
	sprite3DRender->textureHandle = textures_[sprite3DTexIndex_].handle;

	// 2DスプライトUI（ピクセル座標、左上原点）。lightingのデフォルトがfalseな点に注意
	sprite2DObject_.name = "Sprite2D";
	sprite2DObject_.transform.translation = { 100.0f, 100.0f, 0.0f };
	sprite2DObject_.transform.scale = { 200.0f, 200.0f, 1.0f };
	SpriteRenderComponent* sprite2DRender = sprite2DObject_.AddComponent<SpriteRenderComponent>(/*is3D=*/false);
	sprite2DRender->lighting = false;
	sprite2DRender->textureHandle = textures_[sprite2DTexIndex_].handle;

	bgm.Load("Resources/Audio/BGM.mp3");
	AudioManager::GetInstance().RegisterSound("BGM", &bgm, SoundType::BGM, true);

	modelObject_.name = "Model (OBJ)";
	modelObject_.transform.translation = { 5.0f, 0.0f, 0.0f };
	// player.objはメッシュ自体の実寸がCube等の「単位1.0形状」前提と異なるため、
	// ピッキング用の基準半径を実測値に合わせて個別に設定する（scale=1なのでこの値がそのまま半径になる）
	modelObject_.pickingRadiusHint = 2.0f;
	modelTexIndex_ = 1; // デフォルトで t.png を使用
	ModelRenderComponent* modelRender = modelObject_.AddComponent<ModelRenderComponent>(
		renderer_->LoadModel("Resources/Model", "player.obj"), /*hasAnimation=*/false);
	modelRender->textureHandle = textures_[modelTexIndex_].handle;

	// Assimp導入確認用（FBX読み込みテスト。ボーン+アニメーション付きのHumanModel_ver2.fbxで確認）
	fbxModelObject_.name = "FBX Model";
	fbxModelObject_.transform.translation = { 8.0f, 0.0f, 0.0f };
	// MixamoモデルはFBXのUnitScaleFactorメタデータが1.0のまま実寸(cm相当)で出力されており、
	// 他オブジェクトと同じ単位系に合わせるには実測で0.01倍が丁度良かった
	fbxModelObject_.transform.scale = { 0.01f, 0.01f, 0.01f };
	// ピッキング半径 = pickingRadiusHint * scale なので、実寸(cm相当)の人間サイズに対して
	// scale=0.01を掛けた後に半径1.0m程度になるよう、逆算して100.0を基準値にする
	fbxModelObject_.pickingRadiusHint = 100.0f;
	ModelRenderComponent* fbxModelRender = fbxModelObject_.AddComponent<ModelRenderComponent>(
		renderer_->LoadModel("Resources/Model", "HumanModel_ver2.fbx"), /*hasAnimation=*/true);
	fbxModelRender->textureHandle = textures_[fbxModelTexIndex_].handle;

	// 全ての通常オブジェクトのGameObjectをギズモ選択対象として登録する。
	// name(GameObject::name)がそのままImGuiコンボの表示名になる。
	// sprite2DObject_とgridCubes_は既存通りギズモ対象外のため含めない
	gizmoTargets_ = {
		&cubeObject_, &sphereObject_, &triangleObject_, &floorObject_,
		&sprite3DObject_, &modelObject_, &fbxModelObject_,
	};
}

void Game::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	camera_->HandleInput(deltaTime);

	// 0.5秒ごとに直近フレームの平均FPS/フレーム時間を計算し直す（毎フレーム表示は変動が激しく読みづらいため）
	fpsSampleTimer_ += deltaTime;
	fpsSampleFrames_++;
	constexpr float kFpsSampleInterval = 0.5f;
	if (fpsSampleTimer_ >= kFpsSampleInterval) {
		fpsDisplayValue_    = static_cast<float>(fpsSampleFrames_) / fpsSampleTimer_;
		frameTimeDisplayMs_ = (fpsSampleTimer_ / static_cast<float>(fpsSampleFrames_)) * 1000.0f;
		fpsSampleTimer_  = 0.0f;
		fpsSampleFrames_ = 0;
	}
}

void Game::Render() {
	// ImGuizmo導入確認用（Step0）。ImGui::NewFrame()の後、ImGui::Render()の前に呼ぶ必要がある
	ImGuizmo::BeginFrame();

	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->SetCamera(view, proj, camera_->GetCameraData().position);

	floorObject_.GetComponent<CubeRenderComponent>()->Draw(renderer_, floorObject_.transform);

	// グリッドは全個体が同じ回転・スケールのため、WorldInverseTranspose（回転成分のみに依存）は
	// 個体間で共通。回転が変化した時だけ再計算し、毎フレームの2500回分のInverse+Transpose計算を省く
	if (gridCubeCachedRotation_.x != gridCubeRotation_.x ||
		gridCubeCachedRotation_.y != gridCubeRotation_.y ||
		gridCubeCachedRotation_.z != gridCubeRotation_.z) {
		Matrix4x4 rotationOnlyWorld = TransformMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, gridCubeRotation_, { 0.0f, 0.0f, 0.0f });
		gridCubeWorldInverseTranspose_ = MatrixMath::Transpose(MatrixMath::Inverse(rotationOnlyWorld));
		gridCubeCachedRotation_ = gridCubeRotation_;
	}

	// フラストラムカリング：視錐台の外にあるCubeはDrawCube自体を呼ばずスキップする。
	// グリッドCubeは1辺1.0の立方体（scale=1,1,1固定）なので、包含球半径は対角線の半分 sqrt(3)/2 で近似できる
	constexpr float kGridCubeBoundingRadius = 0.8660254f; // sqrt(3)/2
	Collision::Frustum frustum{};
	if (gridFrustumCullingEnabled_) {
		frustum = Collision::MakeFrustumFromViewProjection(view * proj);
	}

	gridCubesDrawnCount_ = 0;
	for (auto& t : gridCubes_) {
		if (gridFrustumCullingEnabled_) {
			Collision::Sphere bounds{ t.translation, kGridCubeBoundingRadius };
			if (!Collision::SphereFrustum(bounds, frustum)) continue;
		}
		Transform rotated = t;
		rotated.rotation = gridCubeRotation_;
		renderer_->DrawCube(rotated, gridCubeWorldInverseTranspose_, gridCubeColor_, textures_[gridCubeTexIndex_].handle, gridCubeLighting_, gridCubeBlendMode_, gridCubeBlendStrength_, gridCubeAlphaTest_, gridCubeAlphaThreshold_);
		gridCubesDrawnCount_++;
	}


	// マウスピッキング：3Dビュー上の左クリックでギズモ選択対象を切り替える（コンボボックス選択と共存）
	UpdatePicking(view, proj);

	// Blenderライクなギズモ操作："Gizmo"パネルで選んだ1オブジェクトのTransformをドラッグで編集する
	UpdateGizmo(view, proj);

	triangleObject_.GetComponent<TriangleRenderComponent>()->Draw(renderer_, triangleObject_.transform);
	cubeObject_.GetComponent<CubeRenderComponent>()->Draw(renderer_, cubeObject_.transform);
	sphereObject_.GetComponent<SphereRenderComponent>()->Draw(renderer_, sphereObject_.transform);
	modelObject_.GetComponent<ModelRenderComponent>()->Draw(renderer_, modelObject_.transform, deltaTime_);
	fbxModelObject_.GetComponent<ModelRenderComponent>()->Draw(renderer_, fbxModelObject_.transform, deltaTime_);
	sprite3DObject_.GetComponent<SpriteRenderComponent>()->Draw(renderer_, sprite3DObject_.transform);
	sprite2DObject_.GetComponent<SpriteRenderComponent>()->Draw(renderer_, sprite2DObject_.transform);

	// Collider（当たり判定）の可視化：重なっていれば赤、していなければ緑のワイヤーフレームで表示
	DrawColliderGizmos(view, proj);

	// 光源を可視化：direction の逆方向 × 15 の位置に黄色い球、原点へのラインで方向を表示
	{
		const Vector3& d = renderer_->GetLight().GetData().direction;
		float len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
		if (len > 0.001f) {
			// 光源位置 = ライトが向く方向の逆方向に 15 進んだ点
			Vector3 lightPos = { -d.x / len * 15.0f, -d.y / len * 15.0f, -d.z / len * 15.0f };

			// 光源位置に黄色い球（ライティングOFF で常に同じ色）
			Transform lightSphere;
			lightSphere.translation = lightPos;
			lightSphere.scale = { 0.5f, 0.5f, 0.5f };
			renderer_->DrawSphere(lightSphere, { 1.0f, 1.0f, 0.0f, 1.0f }, kTextureNone, false);

			// 光源 → 原点 のラインで照射方向を表示
			renderer_->DrawLine(lightPos, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 1.0f }, view, proj);
		}
	}

	// Point Light を可視化：位置に球を表示（ライティングOFFで色そのまま）
	{
		auto& data = renderer_->GetLight().GetData();
		if (data.enablePoint != 0) {
			Transform pointSphere;
			pointSphere.translation = data.pointPosition;
			pointSphere.scale = { 0.3f, 0.3f, 0.3f };
			Vector4 c = { data.pointColor.x, data.pointColor.y, data.pointColor.z, 1.0f };
			renderer_->DrawSphere(pointSphere, c, kTextureNone, false);
		}
	}

	// Spot Light を可視化：位置に球、照射方向にラインを表示
	{
		auto& data = renderer_->GetLight().GetData();
		if (data.enableSpot != 0) {
			Transform spotSphere;
			spotSphere.translation = data.spotPosition;
			spotSphere.scale = { 0.3f, 0.3f, 0.3f };
			Vector4 c = { data.spotColor.x, data.spotColor.y, data.spotColor.z, 1.0f };
			renderer_->DrawSphere(spotSphere, c, kTextureNone, false);

			const Vector3& sd = data.spotDirection;
			float len = sqrtf(sd.x * sd.x + sd.y * sd.y + sd.z * sd.z);
			if (len > 0.001f) {
				Vector3 tip = {
					data.spotPosition.x + sd.x / len * 3.0f,
					data.spotPosition.y + sd.y / len * 3.0f,
					data.spotPosition.z + sd.z / len * 3.0f
				};
				renderer_->DrawLine(data.spotPosition, tip, c, view, proj);
			}
		}
	}

	//DrawGrid();
	DrawImGui();
}

Transform* Game::GetGizmoTargetTransform() {
	if (gizmoTarget_ == GizmoTarget::kPointLight || gizmoTarget_ == GizmoTarget::kSpotLight) {
		return &lightGizmoScratch_;
	}
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
		return &obj->transform;
	}
	return nullptr;
}

void Game::DrawColliderGizmos(const Matrix4x4& view, const Matrix4x4& proj) {
	constexpr Vector4 kOverlapColor   = { 1.0f, 0.2f, 0.2f, 1.0f }; // 赤
	constexpr Vector4 kNoOverlapColor = { 0.2f, 1.0f, 0.2f, 1.0f }; // 緑

	// 全オブジェクトのColliderを先に集めておく（N×Nの重なり判定を1回で済ませるため）
	struct Entry {
		GameObject* obj;
		SphereColliderComponent* sphere;
		OBBColliderComponent* obb;
		bool overlapping = false;
	};
	std::vector<Entry> entries;
	for (auto* obj : gizmoTargets_) {
		auto* sphere = obj->GetComponent<SphereColliderComponent>();
		auto* obb    = obj->GetComponent<OBBColliderComponent>();
		if (sphere || obb) entries.push_back({ obj, sphere, obb });
	}

	// 総当たりで重なりを判定（オブジェクト数が少ないため計算量は無視できる）
	for (size_t i = 0; i < entries.size(); i++) {
		for (size_t j = i + 1; j < entries.size(); j++) {
			bool hit = false;
			auto& a = entries[i];
			auto& b = entries[j];
			if (a.sphere && b.sphere) {
				hit = Collision::SphereSphere(a.sphere->GetWorldSphere(a.obj->transform), b.sphere->GetWorldSphere(b.obj->transform));
			} else if (a.obb && b.obb) {
				hit = Collision::OBBOBB(a.obb->GetWorldOBB(a.obj->transform), b.obb->GetWorldOBB(b.obj->transform));
			} else if (a.sphere && b.obb) {
				hit = Collision::OBBSphere(b.obb->GetWorldOBB(b.obj->transform), a.sphere->GetWorldSphere(a.obj->transform));
			} else if (a.obb && b.sphere) {
				hit = Collision::OBBSphere(a.obb->GetWorldOBB(a.obj->transform), b.sphere->GetWorldSphere(b.obj->transform));
			}
			if (hit) { a.overlapping = true; b.overlapping = true; }
		}
	}

	// 描画は各コンポーネント自身のDrawWireframeに委譲する（Gameは判定結果の色だけ渡す）
	for (auto& e : entries) {
		Vector4 color = e.overlapping ? kOverlapColor : kNoOverlapColor;
		if (e.sphere) e.sphere->DrawWireframe(renderer_, e.obj->transform, color, view, proj);
		if (e.obb)    e.obb->DrawWireframe(renderer_, e.obj->transform, color, view, proj);
	}
}

// 方向ベクトル → オイラー角(ラジアン、XMMatrixRotationRollPitchYaw規約)。
// 基準方向{0,0,1}から目標方向への回転をクォータニオンで求め、既存のMakeAffineMatrix/Decomposeと
// 整合する経路（回転行列→ImGuizmo::DecomposeMatrixToComponentsでのオイラー角抽出）で変換する。
// ジンバルロック付近では不安定になりうるため、ドラッグ操作中(ImGuizmo::IsUsing()==true)の
// 毎フレーム呼び出しは避けること（呼び出し側で抑制する）
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

void Game::UpdatePicking(const Matrix4x4& view, const Matrix4x4& proj) {
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
		const Transform& t = gizmoTargets_[i]->transform;
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
		gizmoTarget_ = GizmoTarget::kNone; // 通常オブジェクト選択中はkNone扱い（コンボ選択と同じ規約）
		gizmoTargetIndex_ = closestIndex;
	}
	// 何にも当たらなかった場合は現在の選択状態を維持する
}

void Game::UpdateGizmo(const Matrix4x4& view, const Matrix4x4& proj) {
	Transform* target = GetGizmoTargetTransform();
	if (!target) return;

	// PointLight/SpotLightの場合、操作開始前に現在値をlightGizmoScratch_へ反映する。
	// ドラッグ中（ImGuizmo::IsUsing()）は往路変換のやり直しをスキップし、直前の値を維持する
	// （方向→回転の逆算はジンバルロック付近で不安定なため、ドラッグ中に再計算すると暴れる）
	auto& light = renderer_->GetLight();
	auto& lightData = light.GetData();
	if (!ImGuizmo::IsUsing()) {
		if (gizmoTarget_ == GizmoTarget::kPointLight) {
			lightGizmoScratch_.translation = lightData.pointPosition;
		} else if (gizmoTarget_ == GizmoTarget::kSpotLight) {
			lightGizmoScratch_.translation = lightData.spotPosition;
			lightGizmoScratch_.rotation    = DirectionToEulerRadians(lightData.spotDirection);
		}
	}

	// Collider編集モードの場合、操作開始前に現在のoffset/radius(またはhalfSize)を
	// colliderGizmoScratch_へ反映する（ドラッグ中は再計算しない、上記ライトと同じ理由）
	GameObject* gizmoTargetObj = (gizmoTargetIndex_ >= 0 && gizmoTargetIndex_ < static_cast<int>(gizmoTargets_.size()))
		? gizmoTargets_[gizmoTargetIndex_] : nullptr;
	if (editCollider_ && gizmoTargetObj && !ImGuizmo::IsUsing()) {
		if (auto* sphereCol = gizmoTargetObj->GetComponent<SphereColliderComponent>()) {
			colliderGizmoScratch_.translation = gizmoTargetObj->transform.translation + sphereCol->offset;
			colliderGizmoScratch_.scale       = { sphereCol->radius, sphereCol->radius, sphereCol->radius };
		} else if (auto* obbCol = gizmoTargetObj->GetComponent<OBBColliderComponent>()) {
			colliderGizmoScratch_.translation = gizmoTargetObj->transform.translation + obbCol->offset;
			colliderGizmoScratch_.scale       = { obbCol->halfSize.x * 2.0f, obbCol->halfSize.y * 2.0f, obbCol->halfSize.z * 2.0f };
		}
	}

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(0, 0, (float)renderer_->GetClientWidth(), (float)renderer_->GetClientHeight());

	Matrix4x4 world = TransformMath::MakeAffineMatrix(target->scale, target->rotation, target->translation);

	// PointLightは回転・スケールの概念がないためTranslateのみ、SpotLightはTranslate/Rotateのみに強制する
	ImGuizmo::OPERATION operation = gizmoOperation_;
	if (gizmoTarget_ == GizmoTarget::kPointLight) {
		operation = ImGuizmo::TRANSLATE;
	} else if (gizmoTarget_ == GizmoTarget::kSpotLight && gizmoOperation_ == ImGuizmo::SCALE) {
		operation = ImGuizmo::TRANSLATE;
	}
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

		// ライトの場合はSetter経由で書き戻す（Upload()を確実に通す。GetData()の非const参照を
		// 直接書き換えるとUpload()が呼ばれずGPUに反映されない罠があるため必ずSetter経由にする）
		if (gizmoTarget_ == GizmoTarget::kPointLight) {
			light.SetPointPosition(target->translation);
		} else if (gizmoTarget_ == GizmoTarget::kSpotLight) {
			light.SetSpotPosition(target->translation);
			light.SetSpotDirection(TransformMath::EulerRadiansToDirection(target->rotation));
		}

		// Collider編集中の場合、ワールド座標系のtranslation/scaleをoffset/radius(またはhalfSize)へ変換して書き戻す
		if (editCollider_ && gizmoTargetObj) {
			if (auto* sphereCol = gizmoTargetObj->GetComponent<SphereColliderComponent>()) {
				sphereCol->offset = target->translation - gizmoTargetObj->transform.translation;
				sphereCol->radius = target->scale.x; // Scaleギズモは等方的なドラッグを想定し、xの値を採用
			} else if (auto* obbCol = gizmoTargetObj->GetComponent<OBBColliderComponent>()) {
				obbCol->offset   = target->translation - gizmoTargetObj->transform.translation;
				obbCol->halfSize = { target->scale.x * 0.5f, target->scale.y * 0.5f, target->scale.z * 0.5f };
			}
		}
	}
}

void Game::DrawGrid() {
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

void Game::DrawImGui() {

	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.1f (0.5s avg)", fpsDisplayValue_);
	ImGui::Text("frameTime: %.3f ms (0.5s avg)", frameTimeDisplayMs_);
	ImGui::Text("Instantaneous FPS: %.1f", 1.0f / deltaTime_);
	ImGui::End();

	ImGui::Begin("Camera");
	ImGui::Text("move wasd");
	ImGui::Text("rotate mouse rightbutton + move mouse");
	ImGui::Text("zoom mouse wheel or up/down arrow");
	ImGui::Text("pos.x = %.1f", camera_->GetCameraData().position.x);
	ImGui::Text("pos.y = %.1f", camera_->GetCameraData().position.y);
	ImGui::Text("pos.z= %.1f", camera_->GetCameraData().position.z);
	ImGui::End();

	ImGui::Begin("Gizmo");
	// コンボの選択肢：0="None", 1..N=gizmoTargets_[0..N-1]の名前, N+1="Point Light", N+2="Spot Light"
	int numTargets = static_cast<int>(gizmoTargets_.size());
	std::vector<const char*> comboNames;
	comboNames.push_back("None");
	for (auto* obj : gizmoTargets_) comboNames.push_back(obj->name.c_str());
	comboNames.push_back("Point Light");
	comboNames.push_back("Spot Light");

	// 現在の選択状態(gizmoTarget_/gizmoTargetIndex_)をコンボの単一インデックスに変換
	int currentCombo = 0; // "None"
	if (gizmoTarget_ == GizmoTarget::kPointLight)     currentCombo = numTargets + 1;
	else if (gizmoTarget_ == GizmoTarget::kSpotLight) currentCombo = numTargets + 2;
	else if (gizmoTargetIndex_ >= 0)                  currentCombo = gizmoTargetIndex_ + 1;

	if (ImGui::Combo("Target", &currentCombo, comboNames.data(), static_cast<int>(comboNames.size()))) {
		if (currentCombo == 0) {
			gizmoTarget_ = GizmoTarget::kNone;
			gizmoTargetIndex_ = -1;
		} else if (currentCombo == numTargets + 1) {
			gizmoTarget_ = GizmoTarget::kPointLight;
			gizmoTargetIndex_ = -1;
		} else if (currentCombo == numTargets + 2) {
			gizmoTarget_ = GizmoTarget::kSpotLight;
			gizmoTargetIndex_ = -1;
		} else {
			// 通常オブジェクト選択中はkNone扱い（PointLight/SpotLightの特殊分岐に入らないようにする）
			gizmoTarget_ = GizmoTarget::kNone;
			gizmoTargetIndex_ = currentCombo - 1;
		}
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

	// PointLight/SpotLightは回転・スケールの概念がない（点光源=Translateのみ、
	// スポットライトの向き=Rotateのみ）ため、無効な操作モードはグレーアウトする。
	// Collider編集中もRotateには意味がないため同様にグレーアウトする
	bool disableRotate = (gizmoTarget_ == GizmoTarget::kPointLight) || editCollider_;
	bool disableScale  = (gizmoTarget_ == GizmoTarget::kPointLight || gizmoTarget_ == GizmoTarget::kSpotLight);

	if (ImGui::RadioButton("Translate", gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (disableRotate) ImGui::BeginDisabled();
	if (ImGui::RadioButton("Rotate", gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
	if (disableRotate) ImGui::EndDisabled();
	ImGui::SameLine();
	if (disableScale) ImGui::BeginDisabled();
	if (ImGui::RadioButton("Scale", gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;
	if (disableScale) ImGui::EndDisabled();

	// Collider同士の重なりは3Dビュー上のワイヤーフレーム色（赤=重なり/緑=非重なり）で
	// 表示されるため、テキストでの重複表示はしない（DrawColliderGizmos()参照）
	ImGui::End();

	auto textureCombo = [&](const char* label, int& index) {
		if (ImGui::BeginCombo(label, textures_[index].name.c_str())) {
			for (int i = 0; i < (int)textures_.size(); i++) {
				bool selected = (i == index);
				if (ImGui::Selectable(textures_[i].name.c_str(), selected))
					index = i;
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		};

	// BlendMode選択コンボ。表示名の並びは BlendMode.h のenum定義順と対応させること
	static const char* kBlendModeNames[] = { "None", "Normal (Alpha)", "Add", "Subtract", "Multiply", "Screen" };
	auto blendModeCombo = [&](const char* label, BlendMode& mode) {
		int current = static_cast<int>(mode);
		if (ImGui::BeginCombo(label, kBlendModeNames[current])) {
			for (int i = 0; i < static_cast<int>(BlendMode::kCount); i++) {
				bool selected = (i == current);
				if (ImGui::Selectable(kBlendModeNames[i], selected))
					mode = static_cast<BlendMode>(i);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		};

	// ブレンドの強さ（commandList->OMSetBlendFactor()に渡す0〜1の定数）。
	// None/Multiply/ScreenはDestBlendがSrcColor依存かブレンド自体が無効なため、
	// この定数では強さを補間できない（GPU固定機能ブレンダーの制約）のでスライダーを無効化する
	auto blendStrengthSlider = [&](const char* label, BlendMode mode, float& strength) {
		bool effective = (mode == BlendMode::kNormal || mode == BlendMode::kAdd || mode == BlendMode::kSubtract);
		if (!effective) ImGui::BeginDisabled();
		ImGui::SliderFloat(label, &strength, 0.0f, 1.0f);
		if (!effective) ImGui::EndDisabled();
		};

	// 2値抜き(Binary Alpha/αTest): αがしきい値未満のピクセルを描画しない
	auto alphaTestControls = [&](const char* checkboxLabel, const char* sliderLabel, bool& enable, float& threshold) {
		ImGui::Checkbox(checkboxLabel, &enable);
		if (!enable) ImGui::BeginDisabled();
		ImGui::SliderFloat(sliderLabel, &threshold, 0.0f, 1.0f);
		if (!enable) ImGui::EndDisabled();
		};

	ImGui::Begin("Objects");
	ImGui::Text("Floor");
	CubeRenderComponent* floorRender = floorObject_.GetComponent<CubeRenderComponent>();
	ImGui::Checkbox("Floor Lighting", &floorRender->lighting);
	ImGui::ColorEdit4("Floor Color", &floorRender->color.x);
	ImGui::DragFloat3("Floor Scale", &floorObject_.transform.scale.x, 0.1f, 0.1f, 200.0f);
	ImGui::DragFloat3("Floor Rotation", &floorObject_.transform.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Floor Translation", &floorObject_.transform.translation.x, 0.1f, -100.0f, 100.0f);
	textureCombo("Floor Texture", floorTexIndex_);
	floorRender->textureHandle = textures_[floorTexIndex_].handle;
	blendModeCombo("Floor BlendMode", floorRender->blendMode);
	blendStrengthSlider("Floor Blend Strength", floorRender->blendMode, floorRender->blendStrength);
	alphaTestControls("Floor Alpha Test", "Floor Alpha Threshold", floorRender->alphaTest, floorRender->alphaThreshold);

	ImGui::Separator();
	ImGui::Text("Grid Cubes");
	ImGui::Text("Cube Count: %d (Drawn: %d)", static_cast<int>(gridCubes_.size()), gridCubesDrawnCount_);
	if (ImGui::SliderInt("Grid Size (NxNxN)", &gridSize_, 0, kGridSizeMax))
		RebuildGridCubes();
	ImGui::Checkbox("Frustum Culling", &gridFrustumCullingEnabled_);
	ImGui::Checkbox("Grid Lighting", &gridCubeLighting_);
	ImGui::ColorEdit4("Grid Color", &gridCubeColor_.x);
	ImGui::DragFloat3("Grid Rotation", &gridCubeRotation_.x, 0.01f, -3.14f, 3.14f);
	if (ImGui::SliderFloat("Grid Smoothness", &cubeSmoothness_, 0.0f, 1.0f))
		renderer_->SetCubeSmoothness(cubeSmoothness_);
	textureCombo("Grid Texture", gridCubeTexIndex_);
	blendModeCombo("Grid BlendMode", gridCubeBlendMode_);
	blendStrengthSlider("Grid Blend Strength", gridCubeBlendMode_, gridCubeBlendStrength_);
	alphaTestControls("Grid Alpha Test", "Grid Alpha Threshold", gridCubeAlphaTest_, gridCubeAlphaThreshold_);

	ImGui::Separator();
	ImGui::Text("Triangle");
	TriangleRenderComponent* triangleRender = triangleObject_.GetComponent<TriangleRenderComponent>();
	ImGui::Checkbox("Triangle Lighting", &triangleRender->lighting);
	ImGui::ColorEdit4("Triangle Color", &triangleRender->color.x);
	if (ImGui::SliderFloat("Triangle Smoothness", &triangleSmoothness_, 0.0f, 1.0f))
		renderer_->SetTriangleSmoothness(triangleSmoothness_);
	ImGui::DragFloat3("Triangle Scale", &triangleObject_.transform.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Triangle Rotation", &triangleObject_.transform.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Triangle Translation", &triangleObject_.transform.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Triangle Texture", triangleTexIndex_);
	triangleRender->textureHandle = textures_[triangleTexIndex_].handle;
	blendModeCombo("Triangle BlendMode", triangleRender->blendMode);
	blendStrengthSlider("Triangle Blend Strength", triangleRender->blendMode, triangleRender->blendStrength);
	alphaTestControls("Triangle Alpha Test", "Triangle Alpha Threshold", triangleRender->alphaTest, triangleRender->alphaThreshold);

	ImGui::Separator();
	ImGui::Text("Cube");
	CubeRenderComponent* cubeRender = cubeObject_.GetComponent<CubeRenderComponent>();
	ImGui::Checkbox("Cube Lighting", &cubeRender->lighting);
	ImGui::ColorEdit4("Cube Color", &cubeRender->color.x);
	if (ImGui::SliderFloat("Cube Smoothness", &cubeSmoothness_, 0.0f, 1.0f))
		renderer_->SetCubeSmoothness(cubeSmoothness_);
	ImGui::DragFloat3("Cube Scale", &cubeObject_.transform.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Cube Rotation", &cubeObject_.transform.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Cube Translation", &cubeObject_.transform.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Cube Texture", cubeTexIndex_);
	cubeRender->textureHandle = textures_[cubeTexIndex_].handle;
	blendModeCombo("Cube BlendMode", cubeRender->blendMode);
	blendStrengthSlider("Cube Blend Strength", cubeRender->blendMode, cubeRender->blendStrength);
	alphaTestControls("Cube Alpha Test", "Cube Alpha Threshold", cubeRender->alphaTest, cubeRender->alphaThreshold);

	ImGui::Separator();
	ImGui::Text("Sphere");
	if (ImGui::SliderInt("Sphere Subdivision", &sphereSubdivision_, 1, static_cast<int>(Renderer::kSphereMaxSubdivision)))
		renderer_->SetSphereSubdivision(static_cast<uint32_t>(sphereSubdivision_));
	SphereRenderComponent* sphereRender = sphereObject_.GetComponent<SphereRenderComponent>();
	ImGui::Checkbox("Sphere Lighting", &sphereRender->lighting);
	ImGui::ColorEdit4("Sphere Color", &sphereRender->color.x);
	ImGui::DragFloat3("Sphere Scale", &sphereObject_.transform.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Sphere Rotation", &sphereObject_.transform.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Sphere Translation", &sphereObject_.transform.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Sphere Texture", sphereTexIndex_);
	sphereRender->textureHandle = textures_[sphereTexIndex_].handle;
	blendModeCombo("Sphere BlendMode", sphereRender->blendMode);
	blendStrengthSlider("Sphere Blend Strength", sphereRender->blendMode, sphereRender->blendStrength);
	alphaTestControls("Sphere Alpha Test", "Sphere Alpha Threshold", sphereRender->alphaTest, sphereRender->alphaThreshold);

	ImGui::Separator();
	ImGui::Text("Model (OBJ)");
	ModelRenderComponent* modelRender = modelObject_.GetComponent<ModelRenderComponent>();
	ImGui::Checkbox("Model Lighting", &modelRender->lighting);
	ImGui::ColorEdit4("Model Color", &modelRender->color.x);
	ImGui::DragFloat3("Model Scale", &modelObject_.transform.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Model Rotation", &modelObject_.transform.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Model Translation", &modelObject_.transform.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Model Texture", modelTexIndex_);
	modelRender->textureHandle = textures_[modelTexIndex_].handle;
	blendModeCombo("Model BlendMode", modelRender->blendMode);
	blendStrengthSlider("Model Blend Strength", modelRender->blendMode, modelRender->blendStrength);
	alphaTestControls("Model Alpha Test", "Model Alpha Threshold", modelRender->alphaTest, modelRender->alphaThreshold);

	ImGui::Separator();
	ImGui::Text("Model (FBX, Assimp)");
	ModelRenderComponent* fbxModelRender = fbxModelObject_.GetComponent<ModelRenderComponent>();
	ImGui::Checkbox("FBX Model Lighting", &fbxModelRender->lighting);
	ImGui::ColorEdit4("FBX Model Color", &fbxModelRender->color.x);
	ImGui::DragFloat3("FBX Model Scale", &fbxModelObject_.transform.scale.x, 0.001f, 0.001f, 1.0f);
	ImGui::DragFloat3("FBX Model Rotation", &fbxModelObject_.transform.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("FBX Model Translation", &fbxModelObject_.transform.translation.x, 0.1f, -50.0f, 50.0f);
	textureCombo("FBX Model Texture", fbxModelTexIndex_);
	fbxModelRender->textureHandle = textures_[fbxModelTexIndex_].handle;
	blendModeCombo("FBX Model BlendMode", fbxModelRender->blendMode);
	blendStrengthSlider("FBX Model Blend Strength", fbxModelRender->blendMode, fbxModelRender->blendStrength);
	alphaTestControls("FBX Model Alpha Test", "FBX Model Alpha Threshold", fbxModelRender->alphaTest, fbxModelRender->alphaThreshold);

	ImGui::Separator();
	ImGui::Text("Sprite3D (world space)");
	SpriteRenderComponent* sprite3DRender = sprite3DObject_.GetComponent<SpriteRenderComponent>();
	ImGui::Checkbox("Sprite3D Lighting", &sprite3DRender->lighting);
	ImGui::ColorEdit4("Sprite3D Color", &sprite3DRender->color.x);
	ImGui::DragFloat3("Sprite3D Scale", &sprite3DObject_.transform.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Sprite3D Rotation", &sprite3DObject_.transform.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Sprite3D Translation", &sprite3DObject_.transform.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Sprite3D Texture", sprite3DTexIndex_);
	sprite3DRender->textureHandle = textures_[sprite3DTexIndex_].handle;
	blendModeCombo("Sprite3D BlendMode", sprite3DRender->blendMode);
	blendStrengthSlider("Sprite3D Blend Strength", sprite3DRender->blendMode, sprite3DRender->blendStrength);
	alphaTestControls("Sprite3D Alpha Test", "Sprite3D Alpha Threshold", sprite3DRender->alphaTest, sprite3DRender->alphaThreshold);
	ImGui::Text("Sprite3D UV Transform");
	ImGui::DragFloat2("3D UV Offset", &sprite3DRender->uvTransform.offset.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat("3D UV Rotation", &sprite3DRender->uvTransform.rotation, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat2("3D UV Scale", &sprite3DRender->uvTransform.scale.x, 0.01f, 0.01f, 10.0f);

	ImGui::Separator();
	ImGui::Text("Sprite2D (pixel coords)");
	SpriteRenderComponent* sprite2DRender = sprite2DObject_.GetComponent<SpriteRenderComponent>();
	ImGui::Checkbox("Sprite2D Lighting", &sprite2DRender->lighting);
	ImGui::ColorEdit4("Sprite2D Color", &sprite2DRender->color.x);
	ImGui::DragFloat2("Sprite2D Pos (px)", &sprite2DObject_.transform.translation.x, 1.0f, 0.0f, 1920.0f);
	ImGui::DragFloat2("Sprite2D Size (px)", &sprite2DObject_.transform.scale.x, 1.0f, 1.0f, 1920.0f);
	ImGui::DragFloat("Sprite2D Rotation", &sprite2DObject_.transform.rotation.z, 0.01f, -3.14f, 3.14f);
	textureCombo("Sprite2D Texture", sprite2DTexIndex_);
	sprite2DRender->textureHandle = textures_[sprite2DTexIndex_].handle;
	blendModeCombo("Sprite2D BlendMode", sprite2DRender->blendMode);
	blendStrengthSlider("Sprite2D Blend Strength", sprite2DRender->blendMode, sprite2DRender->blendStrength);
	alphaTestControls("Sprite2D Alpha Test", "Sprite2D Alpha Threshold", sprite2DRender->alphaTest, sprite2DRender->alphaThreshold);
	ImGui::Text("Sprite2D UV Transform");
	ImGui::DragFloat2("2D UV Offset", &sprite2DRender->uvTransform.offset.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat("2D UV Rotation", &sprite2DRender->uvTransform.rotation, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat2("2D UV Scale", &sprite2DRender->uvTransform.scale.x, 0.01f, 0.01f, 10.0f);

	ImGui::End();

	AudioManager::GetInstance().DrawImGui();

	auto& light = renderer_->GetLight();
	auto& data = light.GetData();

	ImGui::Begin("Lighting");

	ImGui::Text("Directional Light");
	bool enableDirectional = data.enableDirectional != 0;
	if (ImGui::Checkbox("Enable Directional Light", &enableDirectional)) {
		light.SetEnableDirectional(enableDirectional);
	}
	if (ImGui::SliderFloat3("Direction", &data.direction.x, 0.00f, -1.0f)) {
		light.SetDirection(data.direction);
	}
	if (ImGui::ColorEdit3("Color", &data.color.x)) {
		light.SetColor(data.color);
	}
	if (ImGui::SliderFloat("Ambient", &data.ambient, 0.0f, 1.0f)) {
		light.SetAmbient(data.ambient);
	}
	if (ImGui::SliderFloat("Half Lambert Power", &data.halfLambertPower, 0.1f, 8.0f)) {
		light.SetHalfLambertPower(data.halfLambertPower);
	}

	ImGui::Separator();
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

	ImGui::Separator();
	ImGui::Text("Point Light");
	bool enablePoint = data.enablePoint != 0;
	if (ImGui::Checkbox("Enable Point Light", &enablePoint)) {
		light.SetEnablePointLight(enablePoint);
	}
	if (ImGui::DragFloat3("Point Position", &data.pointPosition.x, 0.05f, -20.0f, 20.0f)) {
		light.SetPointPosition(data.pointPosition);
	}
	if (ImGui::ColorEdit3("Point Color", &data.pointColor.x)) {
		light.SetPointColor(data.pointColor);
	}
	if (ImGui::SliderFloat("Point Intensity", &data.pointIntensity, 0.0f, 5.0f)) {
		light.SetPointIntensity(data.pointIntensity);
	}
	if (ImGui::SliderFloat("Point Radius", &data.pointRadius, 0.1f, 30.0f)) {
		light.SetPointRadius(data.pointRadius);
	}
	if (ImGui::SliderFloat("Point Decay", &data.pointDecay, 0.1f, 4.0f)) {
		light.SetPointDecay(data.pointDecay);
	}

	ImGui::Separator();
	ImGui::Text("Spot Light");
	bool enableSpot = data.enableSpot != 0;
	if (ImGui::Checkbox("Enable Spot Light", &enableSpot)) {
		light.SetEnableSpotLight(enableSpot);
	}
	if (ImGui::DragFloat3("Spot Position", &data.spotPosition.x, 0.05f, -20.0f, 20.0f)) {
		light.SetSpotPosition(data.spotPosition);
	}
	if (ImGui::SliderFloat3("Spot Direction", &data.spotDirection.x, -1.0f, 1.0f)) {
		light.SetSpotDirection(data.spotDirection);
	}
	if (ImGui::ColorEdit3("Spot Color", &data.spotColor.x)) {
		light.SetSpotColor(data.spotColor);
	}
	if (ImGui::SliderFloat("Spot Intensity", &data.spotIntensity, 0.0f, 5.0f)) {
		light.SetSpotIntensity(data.spotIntensity);
	}
	if (ImGui::SliderFloat("Spot Distance", &data.spotDistance, 0.1f, 30.0f)) {
		light.SetSpotDistance(data.spotDistance);
	}
	if (ImGui::SliderFloat("Spot Decay", &data.spotDecay, 0.1f, 4.0f)) {
		light.SetSpotDecay(data.spotDecay);
	}
	bool spotAngleChanged = false;
	spotAngleChanged |= ImGui::SliderFloat("Spot Cos Angle (outer)", &data.spotCosAngle, 0.0f, 0.999f);
	spotAngleChanged |= ImGui::SliderFloat("Spot Cos Falloff Start (inner)", &data.spotCosFalloffStart, 0.0f, 0.999f);
	if (spotAngleChanged) {
		light.SetSpotConeAngles(data.spotCosAngle, data.spotCosFalloffStart);
	}

	ImGui::End();

}
