#include "Game.h"
#include "../Externals/imgui/imgui.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"

void Game::Initialize(DirectXManager* directX, Camera* camera) {
	directX_ = directX;
	camera_ = camera;

	transform1_.scale = { 1.0f, 1.0f, 1.0f };
	transform1_.rotation = { 0.0f, 0.0f, 0.0f };
	transform1_.translation = { 0.0f, 15.0f, 0.0f };

	transform2_.scale = { 1.0f, 1.0f, 1.0f };
	transform2_.rotation = { 3.14159f, 0.0f, 0.0f };
	transform2_.translation = { 0.0f, 15.0f, 0.0f };

	trailParam_.maxParticles = 100;
	trailParam_.lifeTimeMax = 40.0f;
	trailParam_.sizeStart = 1.0f;
	trailParam_.sizeEnd = 0.0f;
	trailParam_.colorStart = Vector4(1.0f, 0.6f, 0.2f, 1.0f);
	trailParam_.colorEnd = Vector4(1.0f, 0.2f, 0.0f, 0.0f);
	trailParam_.spawnDistance = 0.3f;
	trailParam_.spawnRadius = 0.15f;
	trailParam_.fallSpeed = 0.2f;
	trailParam_.goalAreaRadius = 10.0f;
	trailParam_.goalY = 0.0f;

	for (int i = 0; i < kMaxTriangles; i++) {
		triangleTransforms_[i].scale = Vector3(1.0f, 1.0f, 1.0f);
		triangleTransforms_[i].rotation = Vector3(0.0f, 0.0f, 0.0f);
		triangleTransforms_[i].translation = Vector3(0.0f, 70.0f - (i * 3.0f), 0.0f);
		trailParticles_[i].Init(trailParam_);
	}
}

void Game::Update(float deltaTime) {
	transform1_.rotation.y += 60.0f * deltaTime;
	transform2_.rotation.y -= 60.0f * deltaTime;
	camera_->HandleInput(deltaTime);

	for (int i = 0; i < kMaxTriangles; i++) {
		trailParticles_[i].Update(triangleTransforms_[i].translation, triangleTransforms_[i].rotation, deltaTime);
	}
}

void Game::Render() {
	float aspectRatio = camera_->GetAspeRatio(directX_->GetClientWidth(), directX_->GetClientHeight());
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix(aspectRatio);

	directX_->DrawTriangleRender(viewMatrix, projectionMatrix, transform1_, Vector4(1.0f, 1.0f, 0.0f, 1.0f), TextureID::None);
	directX_->DrawTriangleRender(viewMatrix, projectionMatrix, transform2_, Vector4(1.0f, 1.0f, 0.0f, 1.0f), TextureID::None);

	directX_->DrawLineRender(viewMatrix, projectionMatrix, transform1_.translation, transform2_.translation, Vector4(1.0f, 1.0f, 1.0f, 1.0f));

	DrawGrid();

	DrawImGui();

	for (int i = 0; i < kMaxTriangles; i++) {
		for (int index : trailParticles_[i].GetActiveList()) {
			const TrailParticleInfo& p = trailParticles_[i].GetParticles()[index];
			Transform t;
			t.scale = Vector3(p.scale, p.scale, p.scale);
			t.rotation = p.rotation;
			t.translation = p.position;
			directX_->DrawTriangleRender(viewMatrix, projectionMatrix, t, p.color, textureID_);
		}
	}
}

void Game::DrawGrid() {
	float aspectRatio = camera_->GetAspeRatio(directX_->GetClientWidth(), directX_->GetClientHeight());
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(aspectRatio);
	directX_->DrawGridBatch(viewMatrix, projMatrix);
}

void Game::DrawImGui() {
	ImGui::Begin("FPS");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("frameTime: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
	ImGui::End();

	ImGui::Begin("Camera");
	ImGui::Text("move wasd");
	ImGui::Text("rotate mouse rightbutton + move mouse");
	ImGui::Text("zoom mouse wheel or up/down arrow");
	ImGui::End();

	ImGui::Begin("Trail Settings");

	bool changed = false;

	ImGui::SeparatorText("Trail");
	changed |= ImGui::SliderFloat3("Fixed Start Pos", &trailParam_.fixedStartPos.x, -50.0f, 50.0f);
	changed |= ImGui::SliderFloat("Fall Speed", &trailParam_.fallSpeed, 0.01f, 2.0f);
	changed |= ImGui::SliderFloat("Goal Area Radius", &trailParam_.goalAreaRadius, 0.0f, 30.0f);
	changed |= ImGui::SliderFloat("Goal Y", &trailParam_.goalY, -10.0f, 10.0f);

	ImGui::SeparatorText("Particle");
	changed |= ImGui::SliderFloat("Life Time", &trailParam_.lifeTimeMax, 5.0f, 1200.0f);
	changed |= ImGui::SliderFloat("Size Start", &trailParam_.sizeStart, 0.1f, 3.0f);
	changed |= ImGui::SliderFloat("Size End", &trailParam_.sizeEnd, 0.0f, 3.0f);
	changed |= ImGui::SliderFloat("Spawn Distance", &trailParam_.spawnDistance, 0.01f, 2.0f);
	changed |= ImGui::SliderFloat("Spawn Radius", &trailParam_.spawnRadius, 0.0f, 2.0f);
	changed |= ImGui::SliderFloat("Rotation Speed", &trailParam_.rotationSpeed, -0.2f, 0.2f);
	changed |= ImGui::ColorEdit4("Color Start", &trailParam_.colorStart.x);
	changed |= ImGui::ColorEdit4("Color End", &trailParam_.colorEnd.x);
	changed |= ImGui::Combo("Texture", reinterpret_cast<int*>(&textureID_), "None\0Texture1\0");

	if (changed) {
		for (int i = 0; i < kMaxTriangles; i++) {
			trailParticles_[i].SetParameter(trailParam_);
		}
	}

	ImGui::End();
}
