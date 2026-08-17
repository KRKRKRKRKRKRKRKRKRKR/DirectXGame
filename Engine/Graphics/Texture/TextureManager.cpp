#include "TextureManager.h"
#include "../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Utils/Logger.h"
#include "../../Utils/StringUtils.h"
#include "../../../Externals/DirectXTex/d3dx12.h"
#include "../ResourceFactory/ResourceFactory.h"
#include <cassert>
#include <format>

void TextureManager::InitializeDefaultTexture(DescriptorHeaps* heaps) {
	// handle=0（kTextureNone）を白テクスチャとして登録
	Load("Resources/White.png", heaps);
}

TextureHandle TextureManager::Load(const std::string& filePath, DescriptorHeaps* heaps) {
	// 同じパスを二重ロードしない
	auto it = pathToHandle_.find(filePath);
	if (it != pathToHandle_.end()) {
		return it->second;
	}

	DirectX::ScratchImage mipImages = LoadFromFile(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ComPtr<ID3D12Resource> resource = CreateTextureResource(metadata);
	ComPtr<ID3D12Resource> intermediate = UploadTextureData(resource, mipImages);

	TextureHandle handle = RegisterTexture(resource, metadata, intermediate, heaps);
	pathToHandle_[filePath] = handle;

	Logger::Log(std::format("TextureManager: Loaded '{}' -> handle {}\n", filePath, handle));
	return handle;
}

TextureHandle TextureManager::CreateFromPixels(uint32_t width, uint32_t height, const uint8_t* rgbaPixels, DescriptorHeaps* heaps) {
	DirectX::ScratchImage image{};
	HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
	assert(SUCCEEDED(hr));

	const DirectX::Image* dstImage = image.GetImage(0, 0, 0);
	const uint8_t* src = rgbaPixels;
	uint8_t* dst = dstImage->pixels;
	const size_t srcRowBytes = static_cast<size_t>(width) * 4;
	for (uint32_t y = 0; y < height; ++y) {
		memcpy(dst + y * dstImage->rowPitch, src + y * srcRowBytes, srcRowBytes);
	}

	const DirectX::TexMetadata& metadata = image.GetMetadata();
	ComPtr<ID3D12Resource> resource = CreateTextureResource(metadata);
	ComPtr<ID3D12Resource> intermediate = UploadTextureData(resource, image);

	TextureHandle handle = RegisterTexture(resource, metadata, intermediate, heaps);
	Logger::Log(std::format("TextureManager: Created texture from pixels ({}x{}) -> handle {}\n", width, height, handle));
	return handle;
}

bool TextureManager::UpdatePixels(TextureHandle handle, uint32_t width, uint32_t height, const uint8_t* rgbaPixels) {
	auto it = textures_.find(handle);
	if (it == textures_.end()) return false;

	TextureResource& texRes = it->second;
	if (texRes.metadata.width != width || texRes.metadata.height != height) return false;

	DirectX::ScratchImage image{};
	HRESULT hr = image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
	assert(SUCCEEDED(hr));

	const DirectX::Image* dstImage = image.GetImage(0, 0, 0);
	const size_t srcRowBytes = static_cast<size_t>(width) * 4;
	for (uint32_t y = 0; y < height; ++y) {
		memcpy(dstImage->pixels + y * dstImage->rowPitch, rgbaPixels + y * srcRowBytes, srcRowBytes);
	}

	// 前フレームの描画でGENERIC_READ状態になっているはずなので、上書きのためCOPY_DESTへ戻す。
	// UploadTextureData側が最後にCOPY_DEST→GENERIC_READへの遷移まで行ってくれる
	D3D12_RESOURCE_BARRIER toCopyDest{};
	toCopyDest.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopyDest.Transition.pResource   = texRes.resource.Get();
	toCopyDest.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	toCopyDest.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
	toCopyDest.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
	commandList_->ResourceBarrier(1, &toCopyDest);

	// 古いintermediateResourceをこのフレーム中は生存させたまま新しい方に差し替える。
	// GPUがまだ前回のコピー（このフレームの直前にCreateFromPixelsで積んだ分等）を完了して
	// いない状態でここに来ると、即座に上書き（=参照カウント0で解放）するとGPUが参照中の
	// リソースを消してしまい D3D12 ERROR #921 OBJECT_DELETED_WHILE_STILL_IN_USE になる
	pendingIntermediateResources_.push_back(texRes.intermediateResource);
	texRes.intermediateResource = UploadTextureData(texRes.resource, image);
	return true;
}

TextureHandle TextureManager::RegisterTexture(
	ComPtr<ID3D12Resource> resource,
	const DirectX::TexMetadata& metadata,
	ComPtr<ID3D12Resource> intermediate,
	DescriptorHeaps* heaps)
{
	TextureHandle handle = nextHandle_++;

	// descriptorHeapIndex はハンドルそのもの（0番はkTextureNone=白テクスチャが使う）。
	// kMaxTextureCountを超えると他オブジェクト用のSRVスロットと衝突するため上限チェックする
	uint32_t heapIndex = handle;
	if (heapIndex >= kMaxTextureCount) {
		Logger::Log(std::format("TextureManager: texture count exceeds kMaxTextureCount({})\n", kMaxTextureCount));
		assert(false);
	}

	auto handles = heaps->CreateTextureSRV_new(
		device_,
		resource.Get(),
		metadata.format,
		static_cast<UINT>(metadata.mipLevels),
		heapIndex
	);

	TextureResource texRes;
	texRes.resource             = resource;
	texRes.intermediateResource = intermediate;
	texRes.metadata             = metadata;
	texRes.srvCpuHandle         = handles.cpuHandle;
	texRes.srvGpuHandle         = handles.gpuHandle;
	texRes.descriptorHeapIndex  = heapIndex;

	textures_[handle] = texRes;
	return handle;
}

ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width            = UINT(metadata.width);
	resourceDesc.Height           = UINT(metadata.height);
	resourceDesc.MipLevels        = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format           = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));
	return resource;
}

ComPtr<ID3D12Resource> TextureManager::CreateDepthStencilTextureResource(int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width            = UINT(width);
	resourceDesc.Height           = UINT(height);
	resourceDesc.MipLevels        = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));
	return resource;
}

DirectX::ScratchImage TextureManager::LoadFromFile(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtils::ConvertString(filePath);

	HRESULT hr = DirectX::LoadFromWICFile(
		filePathW.c_str(),
		DirectX::WIC_FLAGS_DEFAULT_SRGB,
		nullptr,
		image
	);
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImage{};
	hr = DirectX::GenerateMipMaps(
		image.GetImages(),
		image.GetImageCount(),
		image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB,
		0,
		mipImage
	);
	assert(SUCCEEDED(hr));

	return mipImage;
}

ComPtr<ID3D12Resource> TextureManager::UploadTextureData(
	ComPtr<ID3D12Resource> textureResource,
	const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(
		device_,
		mipImages.GetImages(),
		mipImages.GetImageCount(),
		mipImages.GetMetadata(),
		subresources
	);

	uint64_t intermediateSize = GetRequiredIntermediateSize(
		textureResource.Get(), 0, UINT(subresources.size()));

	ComPtr<ID3D12Resource> intermediateResource =
		ResourceFactory::CreateBufferResource(device_, intermediateSize);

	UpdateSubresources(
		commandList_,
		textureResource.Get(),
		intermediateResource.Get(),
		0, 0,
		UINT(subresources.size()),
		subresources.data()
	);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource   = textureResource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList_->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

void TextureManager::InitializeDepthStencil(int32_t width, int32_t height, DescriptorHeaps* heaps) {
	depthStencilResource_ = CreateDepthStencilTextureResource(width, height);
	heaps->CreateDSV(device_, depthStencilResource_.Get());
}

ComPtr<ID3D12Resource> TextureManager::CreateRenderTargetTextureResource(int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width            = UINT(width);
	resourceDesc.Height           = UINT(height);
	resourceDesc.MipLevels        = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 最適化クリア値のフォーマットはRTV作成時に使うUNORM_SRGBに合わせる。異なるとClearRenderTargetViewで
	// D3D12 WARNING #820 (CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE) が発生する
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format   = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	clearValue.Color[0] = 0.0f;
	clearValue.Color[1] = 0.0f;
	clearValue.Color[2] = 0.0f;
	clearValue.Color[3] = 1.0f;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));
	return resource;
}

TextureHandle TextureManager::CreateMirrorRenderTargetTexture(int32_t width, int32_t height, DescriptorHeaps* heaps) {
	mirrorColorResource_ = CreateRenderTargetTextureResource(width, height);

	TextureHandle handle = nextHandle_++;
	uint32_t heapIndex = handle;
	if (heapIndex >= kMaxTextureCount) {
		Logger::Log(std::format("TextureManager: texture count exceeds kMaxTextureCount({})\n", kMaxTextureCount));
		assert(false);
	}

	// RTVはDescriptorHeapsのRTVヒープindex 2（0/1はスワップチェイン専用）へ登録
	heaps->CreateRTVAt(device_, mirrorColorResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 2);
	auto srvHandles = heaps->CreateTextureSRV_new(device_, mirrorColorResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1, heapIndex);

	TextureResource texRes;
	texRes.resource            = mirrorColorResource_;
	texRes.srvCpuHandle        = srvHandles.cpuHandle;
	texRes.srvGpuHandle        = srvHandles.gpuHandle;
	texRes.descriptorHeapIndex = heapIndex;
	// ファイルロード由来のテクスチャと違いmetadata/intermediateResourceは使わないため既定値のまま

	textures_[handle] = texRes;
	mirrorTextureHandle_ = handle;

	Logger::Log(std::format("TextureManager: Created mirror render target -> handle {}\n", handle));
	return handle;
}

void TextureManager::InitializeMirrorDepthStencil(int32_t width, int32_t height, DescriptorHeaps* heaps) {
	mirrorDepthStencilResource_ = CreateDepthStencilTextureResource(width, height);
	// DSVヒープindex 1（0はメイン画面用）へ登録
	heaps->CreateDSVAt(device_, mirrorDepthStencilResource_.Get(), DXGI_FORMAT_D24_UNORM_S8_UINT, 1);
}

void TextureManager::ResizeMirrorRenderTarget(int32_t width, int32_t height, DescriptorHeaps* heaps) {
	if (mirrorTextureHandle_ == kTextureNone) return; // 初期化前（起動シーケンス中の初回リサイズ等）は何もしない

	mirrorColorResource_ = CreateRenderTargetTextureResource(width, height);
	heaps->CreateRTVAt(device_, mirrorColorResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 2);

	TextureResource& texRes = textures_[mirrorTextureHandle_];
	auto srvHandles = heaps->CreateTextureSRV_new(device_, mirrorColorResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1, texRes.descriptorHeapIndex);
	texRes.resource     = mirrorColorResource_;
	texRes.srvCpuHandle = srvHandles.cpuHandle;
	texRes.srvGpuHandle = srvHandles.gpuHandle;

	mirrorDepthStencilResource_ = CreateDepthStencilTextureResource(width, height);
	heaps->CreateDSVAt(device_, mirrorDepthStencilResource_.Get(), DXGI_FORMAT_D24_UNORM_S8_UINT, 1);
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvGpuHandle(TextureHandle handle) const {
	auto it = textures_.find(handle);
	if (it == textures_.end()) {
		Logger::Log(std::format("TextureManager: handle {} not found\n", handle));
		return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	}
	return it->second.srvGpuHandle;
}

bool TextureManager::IsLoaded(TextureHandle handle) const {
	return textures_.count(handle) > 0;
}

void TextureManager::Finalize() {
	for (auto& [handle, texRes] : textures_) {
		texRes.resource.Reset();
		texRes.intermediateResource.Reset();
	}
	textures_.clear();
	pathToHandle_.clear();
	Logger::Log("TextureManager: Finalized\n");
}
