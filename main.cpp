#include "pch.h"
#include "Window/WindowManager.h"
#include "Graphics/DirectXManager.h"
#include "Debug/DebugManager.h"
#include "Utils/Logger.h"
#include "Renderer/PrimitiveRenderer.h"
#include "Math/MathTypes.h"
#include "Math/TransformMath.h"
#include "Math/MatrixMath.h"
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
	primitiveRenderer.Initialize(&directX);
	
	Transform transform;
	transform.scale = { 1.0f, 1.0f, 1.0f };
	transform.rotation = { 0.0f, 0.0f, 0.0f };
	transform.translation = { 0.0f, 0.0f, 0.0f };

	Vector3 cameraPos = { 0.0f, 0.0f, -5.0f };

	while (window.ProcessMessage()) {
		directX.beginFrame();

		transform.rotation.y += 0.1f;
		
		//transform.translation.z += 0.01f;

		//cameraPos.z += 0.01f;

		Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
		Matrix4x4 cameraMatrix = TransformMath::MakeAffineMatrix(Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 0.0f), cameraPos);
		Matrix4x4 viewMatrix = MatrixMath::Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = TransformMath::MakePerspectiveForMatrix(DirectX::XMConvertToRadians(45.0f), static_cast<float>(window.GetClientWidth()) / window.GetClientHeight(), 0.1f, 100.0f);
		Matrix4x4 wvp = worldMatrix * viewMatrix * projectionMatrix;
		Matrix4x4 viewportMatrix = TransformMath::MakeViewPortMatrix(0.0f, 0.0f, static_cast<float>(window.GetClientWidth()), static_cast<float>(window.GetClientHeight()), 0.0f, 1.0f);

		//描画処理はここから
		primitiveRenderer.DrawTriangleRender(&directX, window.GetClientWidth(), window.GetClientHeight(), directX.GetCommandList(), wvp);

		directX.endFrame();
	}

	primitiveRenderer.Finalize();
	directX.Finalize();
	DebugManager::ReportLiveObjects();

	return 0;
}