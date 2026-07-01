#include "ImGuiManager.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

void ImGuiManager::Initialize(HWND hwnd, DirectXManager* dx) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hwnd);
	ID3D12DescriptorHeap* srvHeap = dx->GetSRVDescriptorHeap();
  ImGui_ImplDX12_Init(dx->GetDevice(),
		2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
       srvHeap,
		srvHeap->GetCPUDescriptorHandleForHeapStart(),
		srvHeap->GetGPUDescriptorHandleForHeapStart());
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Build();
}

void ImGuiManager::BeginFrame() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 画面全体を覆うドッキング先（透明・タイトルバーなし）を用意する。
	// 各ウィンドウはここへドラッグ＆ドロップで自由にくっつけられる。
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
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