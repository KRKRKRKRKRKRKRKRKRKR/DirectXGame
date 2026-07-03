#include "DescriptorHeaps.h"
#include "../Texture/TextureManager.h" // kMaxTextureCount参照用
#include "../../Utils/Logger.h" // DescriptorHeaps が Graphics/DescriptorHeaps/ にあるので ../../Utils/
#include <cassert>
void DescriptorHeaps::Initialize(ID3D12Device* device) {
	// index 0/1はスワップチェインのバックバッファ、index 2は鏡の反射用オフスクリーンRTV
	rtvDescriptorHeap_ = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3, false);
	// index 0〜(kMaxTextureCount-1): テクスチャ専用帯（TextureManager::RegisterTexture、
	// handleをそのままheapIndexとして使う自己完結の仕組み）
	// index kMaxTextureCount〜: AllocateSRVIndexで払い出すTriangle/Cube/Sphere/Sprite/Model等の帯
	srvDescriptorHeap_ = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kSrvHeapCapacity, true);
	// index 0はメイン画面用、index 1は鏡の反射用オフスクリーン深度
	dsvDescriptorHeap_ = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 2, false);
	SetDescriptorSizes(device);
	nextSrvAllocIndex_ = kMaxTextureCount;
}

uint32_t DescriptorHeaps::AllocateSRVIndex(uint32_t count) {
	uint32_t allocated = nextSrvAllocIndex_;
	nextSrvAllocIndex_ += count;
	assert(nextSrvAllocIndex_ <= kSrvHeapCapacity && "DescriptorHeaps: SRV heap index exhausted");
	return allocated;
}

void DescriptorHeaps::Finalize() {
	rtvDescriptorHeap_.Reset();
	srvDescriptorHeap_.Reset();
	dsvDescriptorHeap_.Reset();
	textureSrvHandles_.clear();
}

void DescriptorHeaps::CreateRTV(ID3D12Device* device, ID3D12Resource* resource0, ID3D12Resource* resource1) {
	CreateRTVAt(device, resource0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0);
	CreateRTVAt(device, resource1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
}

void DescriptorHeaps::CreateDSV(ID3D12Device* device, ID3D12Resource* depthStencilResource) {
	CreateDSVAt(device, depthStencilResource, DXGI_FORMAT_D24_UNORM_S8_UINT, 0);
}

void DescriptorHeaps::CreateRTVAt(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, uint32_t index) {
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += index * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	rtvHandles_[index] = handle;
	device->CreateRenderTargetView(resource, &rtvDesc, handle);
}

void DescriptorHeaps::CreateDSVAt(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, uint32_t index) {
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = format;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += index * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	dsvHandles_[index] = handle;
	device->CreateDepthStencilView(resource, &dsvDesc, handle);
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

DescriptorHeaps::SrvHandle DescriptorHeaps::CreateStructuredBufferSRV(ID3D12Device* device, ID3D12Resource* resource, uint32_t numElements, uint32_t stride, uint32_t heapIndex)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = stride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = GetCPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, heapIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = GetGPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, heapIndex);

	device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);

	SrvHandle handles = { cpuHandle, gpuHandle };
	textureSrvHandles_[heapIndex] = handles;
	return handles;
}