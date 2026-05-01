#include "pch.h"
#include "Window/WindowManager.h"
#include "Graphics/DirectXManager.h"
#include "Debug/DebugManager.h"
#include "Utils/Logger.h"
#include "Renderer/PrimitiveRenderer.h"
#include "Math/MathTypes.h"
#include "Math/TransformMath.h"
#include "Math/MatrixMath.h"
#include "Camera/CameraManager.h"

std::wstring KwindowTitle = L"DirectXGame";

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	DebugManager::RegisterCrashHandler();

	Logger::Initialize();

	WindowManager window;
	window.Create(KwindowTitle, 1280, 720);

	DebugManager::EnableDebugLayer();

	DirectXManager directX;
	directX.Initialize(window.GetHWND(), window.GetClientWidth(), window.GetClientHeight());

	DebugManager::SetupInfoQueue(directX.GetDevice());

	PrimitiveRenderer primitiveRenderer;
	primitiveRenderer.Initialize(&directX, window.GetClientWidth(), window.GetClientHeight());
	
	CameraManager camera;
	camera.Initialize();

	Transform transform;
	transform.scale = { 1.0f, 1.0f, 1.0f };
	transform.rotation = { 0.0f, 0.0f, 0.0f };
	transform.translation = { 0.0f, 0.0f, 0.0f };

	Vector3 cameraPos = { 0.0f, 0.0f, -5.0f };

	while (window.ProcessMessage()) {
		directX.beginFrame();

		transform.rotation.y += 0.1f;
		
		float aspectRatio = static_cast<float>(window.GetClientWidth()) / static_cast<float>(window.GetClientHeight());

		Matrix4x4 viewMatrix = camera.GetViewMatrix();
		Matrix4x4 projectionMatrix = camera.GetProjectionMatrix(aspectRatio);


		//描画処理はここから

		primitiveRenderer.DrawTriangleRender(viewMatrix, projectionMatrix, transform);

		directX.endFrame();
	}

	primitiveRenderer.Finalize();
	directX.Finalize();
	DebugManager::ReportLiveObjects();

	return 0;
}