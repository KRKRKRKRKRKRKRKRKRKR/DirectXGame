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
	InputDevice::GetInstance().Initialize(window_.GetInstance(), window_.GetHWND());
	Debug::SetupInfoQueue(directX_.GetDevice());
	camera_.Initialize();
	imgui_.Initialize(window_.GetHWND(), &directX_);

	transform1_.scale = { 1.0f, 1.0f, 1.0f };
	transform1_.rotation = { 0.0f, 0.0f, 0.0f };
	transform1_.translation = { 0.0f, 0.0f, 0.0f };
	transform2_.scale = { 1.0f, 1.0f, 1.0f };
	transform2_.rotation = { 0.0f, 0.0f, 0.0f };
	transform2_.translation = { 0.0f, 0.0f, 0.0f };
	cameraData_.position = Vector3(0.0f, 0.0f, -5.0f);
}

void Engine::Run() {
	while (window_.ProcessMessage()) {
		InputDevice::GetInstance().Update();
		directX_.BeginFrame();
		imgui_.BeginFrame();
		Update();
		Render();
		imgui_.EndFrame(&directX_);
		directX_.EndFrame();
	}
}

void Engine::Finalize() {
	imgui_.Finalize();
	InputDevice::GetInstance().Finalize();
	directX_.Finalize();
}

void Engine::Update() {
	CameraControl();
	camera_.SetPosition(cameraData_.position);
	camera_.SetRotation(cameraData_.rotation);
	camera_.SetFov(cameraData_.fov);
	transform2_.rotation.z += 0.01f;
	transform2_.rotation.x += 0.01f;
	transform2_.rotation.y += 0.01f;
}

void Engine::Render() {
	
	float aspectRatio = camera_.GetAspeRatio(window_.GetClientWidth(), window_.GetClientHeight());
	Matrix4x4 viewMatrix = camera_.GetViewMatrix();
	Matrix4x4 projectionMatrix = camera_.GetProjectionMatrix(aspectRatio);
	Matrix4x4 viewMatrixSprite = MatrixMath::Identity();
	Matrix4x4 projectionMatrixSprite = TransformMath::MakeOrthographicMatrix(0, 0, static_cast<float>(window_.GetClientWidth()), static_cast<float>(window_.GetClientHeight()), 0.1f, 100.0f);

	directX_.DrawTriangleRender(viewMatrix, projectionMatrix, transform1_);
	directX_.DrawTriangleRender(viewMatrix, projectionMatrix, transform2_);

	ImGui::Begin("Settings");
	ImGui::SliderFloat3("Object1 Position", &transform1_.translation.x, -1.0f, 1.0f);
	ImGui::SliderFloat3("Object2 Position", &transform2_.translation.x, -1.0f, 1.0f);
	ImGui::End();

}

void Engine::CameraControl() {
	//右クリックが押されている間だけカメラを操作
	if (Input::IsMouseRightPressed()) {
		const float mouseSensitivity = 0.004f;

		long mouseX = Input::GetMouseMoveX();
		long mouseY = Input::GetMouseMoveY();

		// マウスの移動量をカメラの回転に加算
		cameraData_.rotation.y += static_cast<float>(mouseX) * mouseSensitivity;
		cameraData_.rotation.x += static_cast<float>(mouseY) * mouseSensitivity;

		// 上下の角度制限（クランプ）
		if (cameraData_.rotation.x > 1.55f)  cameraData_.rotation.x = 1.55f;
		if (cameraData_.rotation.x < -1.55f) cameraData_.rotation.x = -1.55f;

		long wheel = Input::GetMouseWheel();
		if (wheel != 0) {
			// ホイール感度の調整
			const float zoomSensitivity = 0.1f;

			// 視野角（FOV）を変化させる場合（値を小さくするとズームイン、大きくするとズームアウト）
			// ※ cameraData_.fov の初期値は一般的に 45度（約0.785f）など
			cameraData_.fov -= static_cast<float>(wheel) * zoomSensitivity;

			// 視野角が狭くなりすぎたり（拡大しすぎ）、広くなりすぎたり（魚眼レンズ化）しないよう制限
			// 約10度〜90度の範囲に制限する例
			if (cameraData_.fov < 10) cameraData_.fov = 10;
			if (cameraData_.fov > 90)  cameraData_.fov = 90;
		}
	}

	// --- キーボードによる移動 ---
	Vector3 localMove = { 0.0f, 0.0f, 0.0f };

	if (Input::IsPressed(DIK_W)) { localMove.z += 0.1f; }
	if (Input::IsPressed(DIK_S)) { localMove.z -= 0.1f; }
	if (Input::IsPressed(DIK_A)) { localMove.x -= 0.1f; }
	if (Input::IsPressed(DIK_D)) { localMove.x += 0.1f; }

	if (Input::IsPressed(DIK_SPACE)) { cameraData_.position.y += 0.1f; }
	if (Input::IsPressed(DIK_LSHIFT)) { cameraData_.position.y -= 0.1f; }

	if (Input::IsPressed(DIK_UPARROW)) { cameraData_.rotation.x -= 0.05f; }
	if (Input::IsPressed(DIK_DOWNARROW)) { cameraData_.rotation.x += 0.05f; }
	if (Input::IsPressed(DIK_LEFTARROW)) { cameraData_.rotation.y -= 0.05f; }
	if (Input::IsPressed(DIK_RIGHTARROW)) { cameraData_.rotation.y += 0.05f; }

	Matrix4x4 rotMatrix = TransformMath::MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f },
		cameraData_.rotation,
		{ 0.0f, 0.0f, 0.0f }
	);

	Vector3 worldMove = TransformMath::Transform(localMove, rotMatrix);

	cameraData_.position.x += worldMove.x;
	cameraData_.position.y += worldMove.y;
	cameraData_.position.z += worldMove.z;
}