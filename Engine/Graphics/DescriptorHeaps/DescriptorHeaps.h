#pragma once
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include <map>
using Microsoft::WRL::ComPtr;
class DescriptorHeaps {
public:
	DescriptorHeaps() = default;
	~DescriptorHeaps() = default;

	void Initialize(ID3D12Device* device);
	void Finalize();

	void CreateRTV(ID3D12Device* device, ID3D12Resource* resource0, ID3D12Resource* resource1);
	void CreateDSV(ID3D12Device* device, ID3D12Resource* depthStencilResource);
	void CreateTextureSRV(ID3D12Device* device, ID3D12Resource* textureResource, DXGI_FORMAT format, UINT mipLevels, uint32_t heapIndex);

	
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetRTVDescriptorHeap() const { return rtvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetDSVDescriptorHeap() const { return dsvDescriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index) const { return rtvHandles_[index]; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandle_; }

	uint32_t GetDescriptorSizeSRV() const { return descriptorSizeSRV_; }
	uint32_t GetDescriptorSizeRTV() const { return descriptorSizeRTV_; }
	uint32_t GetDescriptorSizeDSV() const { return descriptorSizeDSV_; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index);

	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvHandleByIndex(uint32_t heapIndex) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetTextureSrvCpuHandleByIndex(uint32_t heapIndex) const;

	struct SrvHandle {
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};

	SrvHandle CreateTextureSRV_new(ID3D12Device* device, ID3D12Resource* textureResource, DXGI_FORMAT format, UINT mipLevels, uint32_t heapIndex);
	SrvHandle CreateStructuredBufferSRV(ID3D12Device* device, ID3D12Resource* resource, uint32_t numElements, uint32_t stride, uint32_t heapIndex);
private:
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	void SetDescriptorSizes(ID3D12Device* device);

	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2] = {};
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};
	std::map<uint32_t, SrvHandle> textureSrvHandles_;
	uint32_t descriptorSizeSRV_ = 0;
	uint32_t descriptorSizeRTV_ = 0;
	uint32_t descriptorSizeDSV_ = 0;
};