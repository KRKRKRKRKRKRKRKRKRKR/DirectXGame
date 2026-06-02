#include "DescriptorHeaps.h"
#include "../../Utils/Logger.h"
#include <cassert>
void DescriptorHeaps::Initialize(ID3D12Device* device) {
	rtvDescriptorHeap_ = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
   srvDescriptorHeap_ = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	dsvDescriptorHeap_ = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	SetDescriptorSizes(device);
}

void DescriptorHeaps::Finalize() {
	rtvDescriptorHeap_.Reset();
	srvDescriptorHeap_.Reset();
	dsvDescriptorHeap_.Reset();
	textureSrvHandles_.clear();
}

void DescriptorHeaps::CreateRTV(ID3D12Device* device, ID3D12Resource* resource0, ID3D12Resource* resource1) {
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvHandles_[0] = rtvStartHandle;
	device->CreateRenderTargetView(resource0, &rtvDesc, rtvHandles_[0]);
	rtvHandles_[1].ptr = rtvHandles_[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(resource1, &rtvDesc, rtvHandles_[1]);
}

void DescriptorHeaps::CreateDSV(ID3D12Device* device, ID3D12Resource* depthStencilResource) {
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
	dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
}

DescriptorHeaps::SrvHandle DescriptorHeaps::CreateTextureSRV_new(
	ID3D12Device* device,
	ID3D12Resource* textureResource,
	DXGI_FORMAT format,
	UINT mipLevels,
	uint32_t heapIndex
) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = mipLevels;

	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = GetCPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, heapIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = GetGPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, heapIndex);

	device->CreateShaderResourceView(textureResource, &srvDesc, srvCpuHandle);

	// === ハンドルを保存（拡張性のため） ===
	SrvHandle handles = { srvCpuHandle, srvGpuHandle };
	textureSrvHandles_[heapIndex] = handles;

	return handles;
}

ComPtr<ID3D12DescriptorHeap> DescriptorHeaps::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
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

void DescriptorHeaps::SetDescriptorSizes(ID3D12Device* device) {
	descriptorSizeSRV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeaps::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += index * descriptorSize;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeaps::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += index * descriptorSize;
	return handle;
}


D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeaps::GetTextureSrvHandleByIndex(uint32_t heapIndex) const {
	auto it = textureSrvHandles_.find(heapIndex);
	if (it != textureSrvHandles_.end()) {
		return it->second.gpuHandle;
	}
	Logger::Log("DescriptorHeaps: Texture SRV handle not found for heapIndex\n");
	return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeaps::GetTextureSrvCpuHandleByIndex(uint32_t heapIndex) const {
	auto it = textureSrvHandles_.find(heapIndex);
	if (it != textureSrvHandles_.end()) {
		return it->second.cpuHandle;
	}
	Logger::Log("DescriptorHeaps: Texture SRV handle not found for heapIndex\n");
	return D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
}
