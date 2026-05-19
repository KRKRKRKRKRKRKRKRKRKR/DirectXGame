#include "DescriptorManager.h"
#include "../../Utils/Logger.h"
#include <cassert>

void DescriptorManager::Initialize(ID3D12Device* device, ID3D12Resource* swapChainResources[2]) {
	device_ = device;
	swapChainResources_[0] = swapChainResources[0];
	swapChainResources_[1] = swapChainResources[1];
	CreateRTVDescriptorHeap();
	CreateSRVDescriptorHeap();
	CreateDSVDescriptorHeap();
}

//===========================================
//Descriptor Heapの作成
//===========================================
ComPtr<ID3D12DescriptorHeap> DescriptorManager::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	// ディスクリプタヒープを設定
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));

	if (FAILED(hr)) {
		Logger::Log("Failed CreateDescriptorHeap\n");
	}

	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

// RTV用のディスクリプタヒープを作成
void DescriptorManager::CreateRTVDescriptorHeap() {
	rtvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
}

// SRV用のディスクリプタヒープを作成
void DescriptorManager::CreateSRVDescriptorHeap() {
	srvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
}

// DSV用のディスクリプタヒープを作成
void DescriptorManager::CreateDSVDescriptorHeap() {
	dsvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}


// RTVの作成
void DescriptorManager::CreateRTV() {
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	// RTVの設定
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvHandles_[0] = rtvStartHandle;
	// スワップチェーンのリソースに対してRTVを作成
	device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);
	rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// スワップチェーンの2番目のリソースに対してRTVを作成
	device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);
}