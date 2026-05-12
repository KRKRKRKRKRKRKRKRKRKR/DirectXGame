#include "ImGuiManager.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

void ImGuiManager::Initialize(HWND hwnd, DirectXManager* dx) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(dx->GetDevice(),
		2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
       dx->GetSRVDescriptorHeap(),
		dx->GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		dx->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();
}

void ImGuiManager::BeginFrame() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiManager::EndFrame(DirectXManager* dx) {
	ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->GetCommandList());

}

void ImGuiManager::Finalize() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}