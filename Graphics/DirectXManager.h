#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class DirectXManager {
public:
	DirectXManager() = default;
	~DirectXManager();

	// 初期化
	void Initialize(HWND hwnd, int32_t width, int32_t height);
	void beginFrame();
	void endFrame();

	// ゲッター
	ID3D12Device* GetDevice()  const { return device_; }
	IDXGIFactory7* GetFactory() const { return dxgiFactory_; }

private:

	// 各初期化処理を分割
	void CreateFactory();
	void SelectAdapter();
	void CreateDevice();
	void CreateCommandQueue();
	void CreateSwapChain(HWND hwnd,int32_t width, int32_t height);
	void CreateDescriptorHeap();
	void GetSwapChainResources();
	void CreateRTV();



	IDXGIFactory7* dxgiFactory_ = nullptr;
	IDXGIAdapter4* useAdapter_ = nullptr;
	ID3D12Device* device_ = nullptr;
	ID3D12CommandQueue* commandQueue_ = nullptr;
	ID3D12CommandAllocator* commandAllocator_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;
	IDXGISwapChain4* swapChain_ = nullptr;
	ID3D12DescriptorHeap* rtvDescriptorHeap_ = nullptr;
	ID3D12Resource* swapChainResources_[2] = { nullptr, nullptr };
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2] = {};

};