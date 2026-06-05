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

	//transform3は上下を逆さまにする
	transform3_.scale = { 1.0f, 1.0f, 1.0f };
	transform3_.rotation = { 3.14159f, 0.0f, 0.0f };
	transform3_.translation = { 0.0f, 0.0f, 0.0f };

	cameraData_.position = Vector3(0.0f, 0.5f, -5.0f);

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
		triangleTransforms_[i].translation = Vector3(0.0f, 30.0f - (i * 3.0f), 0.0f);
		trailParticles_[i].Init(trailParam_);
	}

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
	transform1_.rotation.y += 1.0f;
	transform3_.rotation.y -= 1.0f;
	CameraControl();
	for (int i = 0; i < kMaxTriangles; i++) {
		trailParticles_[i].Update(triangleTransforms_[i].translation, triangleTransforms_[i].rotation);
	}

	camera_.SetPosition(cameraData_.position);
	camera_.SetRotation(cameraData_.rotation);
	camera_.SetFov(cameraData_.fov);
}

void Engine::Render() {

	float aspectRatio = camera_.GetAspeRatio(window_.GetClientWidth(), window_.GetClientHeight());
	Matrix4x4 viewMatrix = camera_.GetViewMatrix();
	Matrix4x4 projectionMatrix = camera_.GetProjectionMatrix(aspectRatio);
	Matrix4x4 viewMatrixSprite = MatrixMath::Identity();
	Matrix4x4 projectionMatrixSprite = TransformMath::MakeOrthographicMatrix(0, 0, static_cast<float>(window_.GetClientWidth()), static_cast<float>(window_.GetClientHeight()), 0.1f, 100.0f);

	directX_.DrawTriangleRender(viewMatrix, projectionMatrix, transform1_);
	directX_.DrawTriangleRender(viewMatrix, projectionMatrix, transform3_);

	directX_.DrawLineRender(viewMatrix, projectionMatrix, transform1_.translation, transform3_.translation, Vector4(1.0f, 1.0f, 1.0f, 1.0f));

	DrawGrid();

	DrawImGui();

	for (int i = 0; i < kMaxTriangles; i++) {
		for (int index : trailParticles_[i].GetActiveList()) {
			const TrailParticleInfo& p = trailParticles_[i].GetParticles()[index];
			Transform t;
			t.scale = Vector3(p.scale, p.scale, p.scale);
			t.rotation = p.rotation;
			t.translation = p.position;
			directX_.DrawTriangleRender(viewMatrix, projectionMatrix, t);
		}
	}

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

void Engine::DrawGrid() {
	const float halfSize = 50.0f;
	const float step = 1.0f;
	const Vector4 colorNormal = { 0.4f, 0.4f, 0.4f, 1.0f };
	const Vector4 colorAxis = { 0.7f, 0.7f, 0.7f, 1.0f }; // 軸線は少し明るく

	float aspectRatio = camera_.GetAspeRatio(window_.GetClientWidth(), window_.GetClientHeight());
	Matrix4x4 viewMatrix = camera_.GetViewMatrix();
	Matrix4x4 projMatrix = camera_.GetProjectionMatrix(aspectRatio);

	// Z方向に伸びる線（X軸方向に並べる）
	for (float x = -halfSize; x <= halfSize; x += step) {
		Vector3 start = { x, 0.0f, -halfSize };
		Vector3 end = { x, 0.0f,  halfSize };
		const Vector4& color = (x == 0.0f) ? colorAxis : colorNormal;
		directX_.DrawLineRender(viewMatrix, projMatrix, start, end, color);
	}

	// X方向に伸びる線（Z軸方向に並べる）
	for (float z = -halfSize; z <= halfSize; z += step) {
		Vector3 start = { -halfSize, 0.0f, z };
		Vector3 end = { halfSize, 0.0f, z };
		const Vector4& color = (z == 0.0f) ? colorAxis : colorNormal;
		directX_.DrawLineRender(viewMatrix, projMatrix, start, end, color);
	}
}

void Engine::DrawImGui() {

	ImGui::Begin("Camera");
	ImGui::Text("move wasd");
	ImGui::Text("rotate mouse rightbutton + move mouse");
	ImGui::Text("zoom mouse wheel or up/down arrow");
	ImGui::End();

	ImGui::Begin("Trail Settings");

	bool changed = false;

	ImGui::SeparatorText("Trail");
	changed |= ImGui::SliderFloat("Fall Speed", &trailParam_.fallSpeed, 0.01f, 2.0f);
	changed |= ImGui::SliderFloat("Goal Area Radius", &trailParam_.goalAreaRadius, 0.0f, 30.0f);
	changed |= ImGui::SliderFloat("Goal Y", &trailParam_.goalY, -10.0f, 10.0f);

	ImGui::SeparatorText("Particle");
	changed |= ImGui::SliderFloat("Life Time", &trailParam_.lifeTimeMax, 5.0f, 120.0f);
	changed |= ImGui::SliderFloat("Size Start", &trailParam_.sizeStart, 0.1f, 3.0f);
	changed |= ImGui::SliderFloat("Size End", &trailParam_.sizeEnd, 0.0f, 3.0f);
	changed |= ImGui::SliderFloat("Spawn Distance", &trailParam_.spawnDistance, 0.01f, 2.0f);
	changed |= ImGui::SliderFloat("Spawn Radius", &trailParam_.spawnRadius, 0.0f, 2.0f);
	changed |= ImGui::SliderFloat("Rotation Speed", &trailParam_.rotationSpeed, -0.2f, 0.2f);

	// パラメータが変わったら全員に反映
	if (changed) {
		for (int i = 0; i < kMaxTriangles; i++) {
			trailParticles_[i].SetParameter(trailParam_);
		}
	}



	ImGui::End();
}