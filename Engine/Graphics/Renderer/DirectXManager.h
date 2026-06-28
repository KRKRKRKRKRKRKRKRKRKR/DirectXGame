#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cstdint>
#include <string>
#include <wrl.h>
#include "../../../Externals/DirectXTex/DirectXTex.h"
#include "../../../Externals/DirectXTex/d3dx12.h"
#include "../DirectXDevice/DirectXDevice.h"
#include "../CommandManager/CommandManager.h"
#include "../SwapChainManager/SwapChainManager.h"

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

class DirectXManager {
public:
	DirectXManager() = default;
	~DirectXManager();

	void Initialize(HWND hwnd, int width, int height);
	void BeginFrame();
	void EndFrame();
	void Finalize();

	// ===== ゲッター =====
	ID3D12Device* GetDevice() const { return device_.GetDevice(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return command_.GetCommandList(); }
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return swapChain_.GetDescriptorHeaps()->GetSRVDescriptorHeap(); }
	DescriptorHeaps* GetDescriptorHeaps() const { return swapChain_.GetDescriptorHeaps(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return swapChain_.GetDescriptorHeaps()->GetDSVHandle(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return swapChain_.GetDescriptorHeaps()->GetRTVHandle(swapChain_.GetCurrentBackBufferIndex()); }
	UINT GetBackBufferIndex() const { return swapChain_.GetCurrentBackBufferIndex(); }
	int32_t GetClientWidth() const { return windowWidth_; }
	int32_t GetClientHeight() const { return windowHeight_; }

	void WaitForGPUCompletion();

private:
	DirectXDevice device_;
	CommandManager command_;
	SwapChainManager swapChain_;

	int windowWidth_ = 0;
	int windowHeight_ = 0;
	bool initialized_ = false;

};
