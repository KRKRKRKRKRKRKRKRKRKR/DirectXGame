#include "Game.h"
#include "../Externals/imgui/imgui.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Engine/Audio/AudioManager.h"
#include <cmath>
//test
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

	sphere.translation = { 3.0f, 1.0f,  0.0f };
	cube.translation = { -3.0f, 1.0f,  0.0f };
	triangle.translation = { -0.0f, 1.0f, 0.0f };

	// 大きなFloor（Cubeを平たく大きく引き伸ばして床として使う）
	floor_.translation = { 0.0f, -0.5f, 0.0f };
	floor_.scale        = { 100.0f, 0.01f, 100.0f };
	floorTexIndex_       = 1;

	constexpr int   kGridSize = 50;
	constexpr float kSpacing = 2.0f;
	constexpr float kOffset = (kGridSize - 1) * kSpacing / 2.0f;
	gridCubes_.reserve(kGridSize * kGridSize);
	for (int z = 0; z < kGridSize; z++) {
		for (int x = 0; x < kGridSize; x++) {
			Transform t;
			t.translation = {
				x * kSpacing - kOffset,
				0.0f,
				z * kSpacing - kOffset
			};
			t.scale = { 1.0f, 1.0f, 1.0f };
			gridCubes_.push_back(t);
		}
	}

	// 3Dスプライト（ワールド空間）
	sprite3D.translation = { 0.0f, 3.0f, 0.0f };

	// 2DスプライトUI（ピクセル座標、左上原点）
	sprite2D.translation = { 100.0f, 100.0f, 0.0f };
	sprite2D.scale = { 200.0f, 200.0f, 1.0f };

	bgm.Load("Resources/Audio/BGM.mp3");
	AudioManager::GetInstance().RegisterSound("BGM", &bgm, SoundType::BGM, true);

	modelHandle_ = renderer_->LoadModel("Resources/Model", "player.obj");
	modelTransform_.translation = { 5.0f, 0.0f, 0.0f };
	modelTexIndex_ = 1; // デフォルトで t.png を使用

	// Assimp導入確認用（FBX読み込みテスト。ボーン+アニメーション付きのHumanModel_ver2.fbxで確認）
	fbxModelHandle_ = renderer_->LoadModel("Resources/Model", "HumanModel_ver2.fbx");
	fbxModelTransform_.translation = { 0.0f, 0.0f, 0.0f };
	// MixamoモデルはFBXのUnitScaleFactorメタデータが1.0のまま実寸(cm相当)で出力されており、
	// 他オブジェクトと同じ単位系に合わせるには実測で0.01倍が丁度良かった
	fbxModelTransform_.scale = { 0.01f, 0.01f, 0.01f };
}

void Game::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	camera_->HandleInput(deltaTime);
	renderer_->UpdateModelAnimation(fbxModelHandle_, deltaTime);
}

void Game::Render() {
	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->SetCamera(view, proj, camera_->GetCameraData().position);

	renderer_->DrawCube(floor_, floorColor_, textures_[floorTexIndex_].handle, floorLighting_, floorBlendMode_, floorBlendStrength_, floorAlphaTest_, floorAlphaThreshold_);

	for (auto& t : gridCubes_) {
		Transform rotated = t;
		rotated.rotation = gridCubeRotation_;
		renderer_->DrawCube(rotated, gridCubeColor_, textures_[gridCubeTexIndex_].handle, gridCubeLighting_, gridCubeBlendMode_, gridCubeBlendStrength_, gridCubeAlphaTest_, gridCubeAlphaThreshold_);
	}

	renderer_->DrawTriangle(triangle, triangleColor, textures_[triangleTexIndex_].handle, triangleLighting, triangleBlendMode_, triangleBlendStrength_, triangleAlphaTest_, triangleAlphaThreshold_);
	renderer_->DrawCube(cube, cubeColor, textures_[cubeTexIndex_].handle, cubeLighting, cubeBlendMode_, cubeBlendStrength_, cubeAlphaTest_, cubeAlphaThreshold_);
	renderer_->DrawSphere(sphere, sphereColor, textures_[sphereTexIndex_].handle, sphereLighting, sphereBlendMode_, sphereBlendStrength_, sphereAlphaTest_, sphereAlphaThreshold_);
	renderer_->DrawModel(modelHandle_, modelTransform_, modelColor_, textures_[modelTexIndex_].handle, modelLighting_, modelBlendMode_, modelBlendStrength_, modelAlphaTest_, modelAlphaThreshold_);
	renderer_->DrawModel(fbxModelHandle_, fbxModelTransform_, fbxModelColor_, textures_[fbxModelTexIndex_].handle, fbxModelLighting_, fbxModelBlendMode_, fbxModelBlendStrength_, fbxModelAlphaTest_, fbxModelAlphaThreshold_);
	renderer_->DrawSprite3D(sprite3D, sprite3DColor, textures_[sprite3DTexIndex_].handle, sprite3DLighting, sprite3DUV, sprite3DBlendMode_, sprite3DBlendStrength_, sprite3DAlphaTest_, sprite3DAlphaThreshold_);
	renderer_->DrawSprite2D(sprite2D, sprite2DColor, textures_[sprite2DTexIndex_].handle, sprite2DLighting, sprite2DUV, sprite2DBlendMode_, sprite2DBlendStrength_, sprite2DAlphaTest_, sprite2DAlphaThreshold_);

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

void Game::DrawGrid() {
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

void Game::DrawImGui() {

	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.1f", 1.0f / deltaTime_);
	ImGui::Text("frameTime: %.3f ms", deltaTime_ * 1000.0f);
	ImGui::End();

	ImGui::Begin("Camera");
	ImGui::Text("move wasd");
	ImGui::Text("rotate mouse rightbutton + move mouse");
	ImGui::Text("zoom mouse wheel or up/down arrow");
	ImGui::Text("pos.x = %.1f", camera_->GetCameraData().position.x);
	ImGui::Text("pos.y = %.1f", camera_->GetCameraData().position.y);
	ImGui::Text("pos.z= %.1f", camera_->GetCameraData().position.z);
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
	ImGui::Checkbox("Floor Lighting", &floorLighting_);
	ImGui::ColorEdit4("Floor Color", &floorColor_.x);
	ImGui::DragFloat3("Floor Scale", &floor_.scale.x, 0.1f, 0.1f, 200.0f);
	ImGui::DragFloat3("Floor Rotation", &floor_.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Floor Translation", &floor_.translation.x, 0.1f, -100.0f, 100.0f);
	textureCombo("Floor Texture", floorTexIndex_);
	blendModeCombo("Floor BlendMode", floorBlendMode_);
	blendStrengthSlider("Floor Blend Strength", floorBlendMode_, floorBlendStrength_);
	alphaTestControls("Floor Alpha Test", "Floor Alpha Threshold", floorAlphaTest_, floorAlphaThreshold_);

	ImGui::Separator();
	ImGui::Text("Grid Cubes");
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
	ImGui::Checkbox("Triangle Lighting", &triangleLighting);
	ImGui::ColorEdit4("Triangle Color", &triangleColor.x);
	if (ImGui::SliderFloat("Triangle Smoothness", &triangleSmoothness_, 0.0f, 1.0f))
		renderer_->SetTriangleSmoothness(triangleSmoothness_);
	ImGui::DragFloat3("Triangle Scale", &triangle.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Triangle Rotation", &triangle.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Triangle Translation", &triangle.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Triangle Texture", triangleTexIndex_);
	blendModeCombo("Triangle BlendMode", triangleBlendMode_);
	blendStrengthSlider("Triangle Blend Strength", triangleBlendMode_, triangleBlendStrength_);
	alphaTestControls("Triangle Alpha Test", "Triangle Alpha Threshold", triangleAlphaTest_, triangleAlphaThreshold_);

	ImGui::Separator();
	ImGui::Text("Cube");
	ImGui::Checkbox("Cube Lighting", &cubeLighting);
	ImGui::ColorEdit4("Cube Color", &cubeColor.x);
	if (ImGui::SliderFloat("Cube Smoothness", &cubeSmoothness_, 0.0f, 1.0f))
		renderer_->SetCubeSmoothness(cubeSmoothness_);
	ImGui::DragFloat3("Cube Scale", &cube.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Cube Rotation", &cube.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Cube Translation", &cube.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Cube Texture", cubeTexIndex_);
	blendModeCombo("Cube BlendMode", cubeBlendMode_);
	blendStrengthSlider("Cube Blend Strength", cubeBlendMode_, cubeBlendStrength_);
	alphaTestControls("Cube Alpha Test", "Cube Alpha Threshold", cubeAlphaTest_, cubeAlphaThreshold_);

	ImGui::Separator();
	ImGui::Text("Sphere");
	ImGui::Checkbox("Sphere Lighting", &sphereLighting);
	ImGui::ColorEdit4("Sphere Color", &sphereColor.x);
	ImGui::DragFloat3("Sphere Scale", &sphere.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Sphere Rotation", &sphere.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Sphere Translation", &sphere.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Sphere Texture", sphereTexIndex_);
	blendModeCombo("Sphere BlendMode", sphereBlendMode_);
	blendStrengthSlider("Sphere Blend Strength", sphereBlendMode_, sphereBlendStrength_);
	alphaTestControls("Sphere Alpha Test", "Sphere Alpha Threshold", sphereAlphaTest_, sphereAlphaThreshold_);

	ImGui::Separator();
	ImGui::Text("Model (OBJ)");
	ImGui::Checkbox("Model Lighting", &modelLighting_);
	ImGui::ColorEdit4("Model Color", &modelColor_.x);
	ImGui::DragFloat3("Model Scale", &modelTransform_.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Model Rotation", &modelTransform_.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Model Translation", &modelTransform_.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Model Texture", modelTexIndex_);
	blendModeCombo("Model BlendMode", modelBlendMode_);
	blendStrengthSlider("Model Blend Strength", modelBlendMode_, modelBlendStrength_);
	alphaTestControls("Model Alpha Test", "Model Alpha Threshold", modelAlphaTest_, modelAlphaThreshold_);

	ImGui::Separator();
	ImGui::Text("Model (FBX, Assimp)");
	ImGui::Checkbox("FBX Model Lighting", &fbxModelLighting_);
	ImGui::ColorEdit4("FBX Model Color", &fbxModelColor_.x);
	ImGui::DragFloat3("FBX Model Scale", &fbxModelTransform_.scale.x, 0.001f, 0.001f, 1.0f);
	ImGui::DragFloat3("FBX Model Rotation", &fbxModelTransform_.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("FBX Model Translation", &fbxModelTransform_.translation.x, 0.1f, -50.0f, 50.0f);
	textureCombo("FBX Model Texture", fbxModelTexIndex_);
	blendModeCombo("FBX Model BlendMode", fbxModelBlendMode_);
	blendStrengthSlider("FBX Model Blend Strength", fbxModelBlendMode_, fbxModelBlendStrength_);
	alphaTestControls("FBX Model Alpha Test", "FBX Model Alpha Threshold", fbxModelAlphaTest_, fbxModelAlphaThreshold_);

	ImGui::Separator();
	ImGui::Text("Sprite3D (world space)");
	ImGui::Checkbox("Sprite3D Lighting", &sprite3DLighting);
	ImGui::ColorEdit4("Sprite3D Color", &sprite3DColor.x);
	ImGui::DragFloat3("Sprite3D Scale", &sprite3D.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Sprite3D Rotation", &sprite3D.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Sprite3D Translation", &sprite3D.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Sprite3D Texture", sprite3DTexIndex_);
	blendModeCombo("Sprite3D BlendMode", sprite3DBlendMode_);
	blendStrengthSlider("Sprite3D Blend Strength", sprite3DBlendMode_, sprite3DBlendStrength_);
	alphaTestControls("Sprite3D Alpha Test", "Sprite3D Alpha Threshold", sprite3DAlphaTest_, sprite3DAlphaThreshold_);
	ImGui::Text("Sprite3D UV Transform");
	ImGui::DragFloat2("3D UV Offset", &sprite3DUV.offset.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat("3D UV Rotation", &sprite3DUV.rotation, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat2("3D UV Scale", &sprite3DUV.scale.x, 0.01f, 0.01f, 10.0f);

	ImGui::Separator();
	ImGui::Text("Sprite2D (pixel coords)");
	ImGui::Checkbox("Sprite2D Lighting", &sprite2DLighting);
	ImGui::ColorEdit4("Sprite2D Color", &sprite2DColor.x);
	ImGui::DragFloat2("Sprite2D Pos (px)", &sprite2D.translation.x, 1.0f, 0.0f, 1920.0f);
	ImGui::DragFloat2("Sprite2D Size (px)", &sprite2D.scale.x, 1.0f, 1.0f, 1920.0f);
	ImGui::DragFloat("Sprite2D Rotation", &sprite2D.rotation.z, 0.01f, -3.14f, 3.14f);
	textureCombo("Sprite2D Texture", sprite2DTexIndex_);
	blendModeCombo("Sprite2D BlendMode", sprite2DBlendMode_);
	blendStrengthSlider("Sprite2D Blend Strength", sprite2DBlendMode_, sprite2DBlendStrength_);
	alphaTestControls("Sprite2D Alpha Test", "Sprite2D Alpha Threshold", sprite2DAlphaTest_, sprite2DAlphaThreshold_);
	ImGui::Text("Sprite2D UV Transform");
	ImGui::DragFloat2("2D UV Offset", &sprite2DUV.offset.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat("2D UV Rotation", &sprite2DUV.rotation, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat2("2D UV Scale", &sprite2DUV.scale.x, 0.01f, 0.01f, 10.0f);

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
