#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <cstdint>
#include <string>
#include "../Externals/DirectXTex/DirectXTex.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class DirectXManager {
public:
	DirectXManager() = default;
	~DirectXManager();

	// 初期化
	void Initialize(HWND hwnd, int32_t width, int32_t height);
	void BeginFrame();
	void EndFrame();
	void Finalize();

	// ゲッター
	ID3D12Device* GetDevice()  const { return device_; }
	IDXGIFactory7* GetFactory() const { return dxgiFactory_; }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_; }
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap_; }
	const D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() const { return descriptorRange_; }
	UINT GetDescriptorRangeCount() const { return DESCRIPTOR_RANGE_COUNT; }
	const D3D12_STATIC_SAMPLER_DESC* GetStaticSamplers() const { return staticSamplers_; }
	UINT GetStaticSamplerCount() const { return STATIC_SAMPLER_COUNT; }
private:

	// 各初期化処理を分割
	void CreateFactory();
	void SelectAdapter();
	void CreateDevice();
	void CreateCommandQueue();
	void CreateSwapChain(HWND hwnd, int32_t width, int32_t height);
	ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	void CreateRTVDescriptorHeap();
	void CreateSRVDescriptorHeap();
	void GetSwapChainResources();
	void CreateRTV();
	void BeginTransitionBarrier();
	void EndTransitionBarrier();
	void CreateFence();

	IDXGIFactory7* dxgiFactory_ = nullptr;
	IDXGIAdapter4* useAdapter_ = nullptr;
	ID3D12Device* device_ = nullptr;
	ID3D12CommandQueue* commandQueue_ = nullptr;
	ID3D12CommandAllocator* commandAllocator_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;
	IDXGISwapChain4* swapChain_ = nullptr;
	ID3D12DescriptorHeap* rtvDescriptorHeap_ = nullptr;
	ID3D12DescriptorHeap* srvDescriptorHeap_ = nullptr;
	ID3D12Resource* swapChainResources_[2] = { nullptr, nullptr };
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2] = {};
	UINT backBufferIndex_;
	D3D12_RESOURCE_BARRIER barrier_{};
	ID3D12Fence* fence_ = nullptr;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	void InitializeCOM();
	void FinalizeCOM();
	DirectX::ScratchImage LoadTexture(const std::string& filePath);
	ID3D12Resource* CreateTextureResource(const DirectX::TexMetadata& metadata);
	void UploadTextureData(ID3D12Resource* textureResource, const DirectX::ScratchImage& mipImages);
	void CreateTextureFromFile(const std::string& filePath);
	void CreateShaderResourceView(const DirectX::TexMetadata& metadata, ID3D12Resource* textureResource);
	void LoadTextureResource(const std::string& filePath);
	ID3D12Resource* textureResource_ = nullptr;
	void CreateDescriptorRange();
	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 1;
	D3D12_DESCRIPTOR_RANGE descriptorRange_[DESCRIPTOR_RANGE_COUNT] = {};
	void CreateStaticSamplers();
	static constexpr UINT STATIC_SAMPLER_COUNT = 1;
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[STATIC_SAMPLER_COUNT] = {};

};