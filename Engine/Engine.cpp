#include "Engine.h"
#include "../Externals/imgui/imgui.h"
#include "../Debug/Debug.h"
#include "../Utils/Logger.h"

void Engine::Initialize(const std::wstring& windowTitle, int width, int height) {
	Debug::RegisterCrashHandler();
	Logger::Initialize();

	window_.Create(windowTitle, width, height);

	Debug::EnableDebugLayer();

	directX_.Initialize(window_.GetHWND(), window_.GetClientWidth(), window_.GetClientHeight());
  Debug::SetupInfoQueue(directX_.GetDevice());
	
	
	camera_.Initialize();
	
	imgui_.Initialize(window_.GetHWND(), &directX_);

	transform_.scale = { 1.0f, 1.0f, 1.0f };
	transform_.rotation = { 0.0f, 0.0f, 0.0f };
	transform_.translation = { 0.0f, 0.0f, 0.0f };

	transformSprite_.scale = { 1.0f, 1.0f, 1.0f };
	transformSprite_.rotation = { 0.0f, 0.0f, 0.0f };
	transformSprite_.translation = { 0.0f, 0.0f, 0.0f };

	transformSphere_.scale = { 1.0f, 1.0f, 1.0f };
	transformSphere_.rotation = { 0.0f, 0.0f, 0.0f };
	transformSphere_.translation = { 0.0f, 0.0f, 0.0f };

	sphereData_.center = Vector3(0.0f, 0.0f, 0.0f);
	sphereData_.radius = 1.0f;
}

void Engine::Run() {
	while (window_.ProcessMessage()) {
		Update();
		Render();
	}
}

void Engine::Finalize() {
	imgui_.Finalize();
	directX_.Finalize();
}

void Engine::Update() {
	transform_.rotation.y += 0.1f;
	transformSphere_.rotation.y += 0.1f;
}

void Engine::Render() {
	directX_.BeginFrame();
	imgui_.BeginFrame();

	float aspectRatio = camera_.GetAspeRatio(window_.GetClientWidth(), window_.GetClientHeight());
	Matrix4x4 viewMatrix = camera_.GetViewMatrix();
	Matrix4x4 projectionMatrix = camera_.GetProjectionMatrix(aspectRatio);

	Matrix4x4 viewMatrixSprite = MatrixMath::Identity();
	Matrix4x4 projectionMatrixSprite = TransformMath::MakeOrthographicMatrix(0,0,static_cast<float>(window_.GetClientWidth()), static_cast<float>(window_.GetClientHeight()), 0.1f, 100.0f);

	directX_.DrawTriangleRender(viewMatrix, projectionMatrix, transform_);
	directX_.DrawSpriteRender(viewMatrixSprite, projectionMatrixSprite, transformSprite_);
	directX_.CreateDrawSphereResource(sphereData_, viewMatrix, projectionMatrix, transformSphere_);
	ImGui::ShowDemoWindow();

	ImGui::Begin("Settings");
	ImGui::SliderFloat3("Object Position", &transform_.translation.x, -10.0f, 10.0f);
	ImGui::SliderFloat3("Sprite Position", &transformSprite_.translation.x, -100.0f, 100.0f);
	ImGui::SliderFloat3("Sphere Position", &transformSphere_.translation.x, -10.0f, 10.0f);
	ImGui::End();
	imgui_.EndFrame(&directX_);
	directX_.EndFrame();
}
