#include "Game.h"
#include "../Externals/imgui/imgui.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"

void Game::Initialize(Renderer* renderer, Camera* camera) {
	renderer_ = renderer;
	camera_ = camera;

	texHandles_[0] = kTextureNone;
	texHandles_[1] = renderer_->LoadTexture("Resources/t.png");
	sphere.translation = { 0.0f,1.0f,0.0f };
	cube.translation = { 3.0f,1.0f,0.0f };
	triangle.translation = { -3.0f,1.0f,0.0f };
}

void Game::Update(float deltaTime) {
	camera_->HandleInput(deltaTime);
}

void Game::Render() {
	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->SetCamera(view, proj);

	renderer_->DrawSphere(sphere, { 1,1,1,1 }, texHandles_[0]);
	renderer_->DrawCube(cube, { 1,1,1,1 }, texHandles_[0]);
	renderer_->DrawTriangle(triangle, {1,1,1,1},texHandles_[0]);
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
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	ImGui::Text("frameTime: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
	ImGui::End();

	ImGui::Begin("Camera");
	ImGui::Text("move wasd");
	ImGui::Text("rotate mouse rightbutton + move mouse");
	ImGui::Text("zoom mouse wheel or up/down arrow");
	ImGui::End();

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
	ImGui::End();

}
