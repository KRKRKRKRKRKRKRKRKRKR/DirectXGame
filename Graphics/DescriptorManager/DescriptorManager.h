#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

class DescriptorManager {
public:
	DescriptorManager() = default;
	~DescriptorManager() = default;
	void Initialize(ID3D12Device* device, ID3D12Resource* swapChainResources[2]);
private:

	//Descriptor Heapの作成
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	void CreateRTVDescriptorHeap();
	void CreateSRVDescriptorHeap();
	void CreateDSVDescriptorHeap();
	void CreateRTV();

	//Descriptor Heapの管理
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2];

	ComPtr<ID3D12Device> device_ = nullptr;
	ComPtr<ID3D12Resource> swapChainResources_[2] = { nullptr, nullptr };



};