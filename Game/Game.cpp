#include "Game.h"
#include "../Externals/imgui/imgui.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Engine/Audio/AudioManager.h"
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
	}) {
		TextureHandle h = renderer_->LoadTexture(path);
		textures_.push_back({ h, path.substr(path.find_last_of('/') + 1) });
	}

	sprite2DTexIndex_ = 1; // t.png をデフォルト
	sprite3DTexIndex_ = 1;
	triangleTexIndex_ = 1;
	cubeTexIndex_ = 1;
	sphereTexIndex_ = 1;

	sphere.translation   = { 0.0f, 1.0f,  0.0f };
	cube.translation     = { 3.0f, 1.0f,  0.0f };
	triangle.translation = { -3.0f, 1.0f, 0.0f };

	constexpr int   kGridSize = 50;
	constexpr float kSpacing  = 2.0f;
	constexpr float kOffset   = (kGridSize - 1) * kSpacing / 2.0f;
	gridCubes_.reserve(kGridSize * kGridSize);
	for (int z = 0; z < kGridSize; z++) {
		for (int x = 0; x < kGridSize; x++) {
			Transform t;
			t.translation = { x * kSpacing - kOffset, 5.0f, z * kSpacing - kOffset };
			gridCubes_.push_back(t);
		}
	}

	// 3Dスプライト（ワールド空間）
	sprite3D.translation = { 0.0f, 3.0f, 0.0f };

	// 2DスプライトUI（ピクセル座標、左上原点）
	sprite2D.translation = { 100.0f, 100.0f, 0.0f };
	sprite2D.scale       = { 200.0f, 200.0f, 1.0f };

	bgm.Load("Resources/Audio/BGM.mp3");
	bgm.Play(true, SoundType::BGM);
	AudioManager::GetInstance().RegisterSound("BGM", &bgm, SoundType::BGM, true);

	modelHandle_ = renderer_->LoadModel("Resources/Model", "player.obj");
	modelTransform_.translation = { 5.0f, 0.0f, 0.0f };
	modelTex_ = textures_[1].handle; // デフォルトで t.png を使用
}

void Game::Update(float deltaTime) {
	deltaTime_ = deltaTime;
	camera_->HandleInput(deltaTime);
}

void Game::Render() {
	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
	camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->SetCamera(view, proj);

	for (auto& t : gridCubes_) {
		renderer_->DrawCube(t, gridCubeColor_, textures_[gridCubeTexIndex_].handle, gridCubeLighting_);
	}

	renderer_->DrawTriangle(triangle, triangleColor, textures_[triangleTexIndex_].handle, triangleLighting);
	renderer_->DrawCube(cube, cubeColor, textures_[cubeTexIndex_].handle, cubeLighting);
	renderer_->DrawSphere(sphere, sphereColor, textures_[sphereTexIndex_].handle, sphereLighting);
	renderer_->DrawModel(modelHandle_, modelTransform_, modelColor_);
	renderer_->DrawSprite3D(sprite3D, sprite3DColor, textures_[sprite3DTexIndex_].handle, sprite3DLighting, sprite3DUV);
	renderer_->DrawSprite2D(sprite2D, sprite2DColor, textures_[sprite2DTexIndex_].handle, sprite2DLighting, sprite2DUV);

	DrawGrid();
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

	ImGui::Begin("Objects");
	ImGui::Text("Grid Cubes");
	ImGui::Checkbox("Grid Lighting", &gridCubeLighting_);
	ImGui::ColorEdit4("Grid Color", &gridCubeColor_.x);
	textureCombo("Grid Texture", gridCubeTexIndex_);

	ImGui::Separator();
	ImGui::Text("Triangle");
	ImGui::Checkbox("Triangle Lighting", &triangleLighting);
	ImGui::ColorEdit4("Triangle Color", &triangleColor.x);
	ImGui::DragFloat3("Triangle Scale", &triangle.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Triangle Rotation", &triangle.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Triangle Translation", &triangle.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Triangle Texture", triangleTexIndex_);

	ImGui::Separator();
	ImGui::Text("Cube");
	ImGui::Checkbox("Cube Lighting", &cubeLighting);
	ImGui::ColorEdit4("Cube Color", &cubeColor.x);
	ImGui::DragFloat3("Cube Scale", &cube.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Cube Rotation", &cube.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Cube Translation", &cube.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Cube Texture", cubeTexIndex_);

	ImGui::Separator();
	ImGui::Text("Sphere");
	ImGui::Checkbox("Sphere Lighting", &sphereLighting);
	ImGui::ColorEdit4("Sphere Color", &sphereColor.x);
	ImGui::DragFloat3("Sphere Scale", &sphere.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Sphere Rotation", &sphere.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Sphere Translation", &sphere.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Sphere Texture", sphereTexIndex_);

	ImGui::Separator();
	ImGui::Text("Sprite3D (world space)");
	ImGui::Checkbox("Sprite3D Lighting", &sprite3DLighting);
	ImGui::ColorEdit4("Sprite3D Color", &sprite3DColor.x);
	ImGui::DragFloat3("Sprite3D Scale", &sprite3D.scale.x, 0.01f, 0.1f, 10.0f);
	ImGui::DragFloat3("Sprite3D Rotation", &sprite3D.rotation.x, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat3("Sprite3D Translation", &sprite3D.translation.x, 0.01f, -10.0f, 10.0f);
	textureCombo("Sprite3D Texture", sprite3DTexIndex_);
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
	ImGui::Text("Sprite2D UV Transform");
	ImGui::DragFloat2("2D UV Offset", &sprite2DUV.offset.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat("2D UV Rotation", &sprite2DUV.rotation, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat2("2D UV Scale", &sprite2DUV.scale.x, 0.01f, 0.01f, 10.0f);

	ImGui::End();

	AudioManager::GetInstance().DrawImGui();

	auto& light = renderer_->GetLight();
	auto& data = light.GetData();

	ImGui::Begin("Lighting");
	if (ImGui::DragFloat3("Direction", &data.direction.x, 0.01f, -1.0f, 1.0f)) {
		light.SetDirection(data.direction);
	}
	if (ImGui::ColorEdit3("Color", &data.color.x)) {
		light.SetColor(data.color);
	}
	if (ImGui::SliderFloat("Ambient", &data.ambient, 0.0f, 1.0f)) {
		light.SetAmbient(data.ambient);
	}
	ImGui::Separator();
	ImGui::Text("Half Lambert");
	if (ImGui::SliderFloat("Power", &data.halfLambertPower, 0.1f, 8.0f)) {
		light.SetHalfLambertPower(data.halfLambertPower);
	}
	ImGui::End();

}
