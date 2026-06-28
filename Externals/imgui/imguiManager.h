#pragma once
#include <windows.h>
#include "../../Engine/Graphics/Renderer/DirectXManager.h"

class ImGuiManager {

public:
	ImGuiManager() = default;
	~ImGuiManager() = default;

	void Initialize(HWND hwnd, DirectXManager* dx);
	void BeginFrame();
	void EndFrame(DirectXManager* dx);
	void Finalize();
};