#include "TextureManager.h"
#include "../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Utils/Logger.h"
#include "../../Utils/StringUtils.h"
#include "../../../Externals/DirectXTex/d3dx12.h"
#include <cassert>
#include <format>

ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));

	return resource;
}

ComPtr<ID3D12Resource> TextureManager::CreateBufferResource(size_t sizeInBytes) {
	ComPtr<ID3D12Resource> resource = nullptr;
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC vertexBufferResourceDesc{};
	vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexBufferResourceDesc.Width = sizeInBytes;
	vertexBufferResourceDesc.Height = 1;
	vertexBufferResourceDesc.DepthOrArraySize = 1;
	vertexBufferResourceDesc.MipLevels = 1;
	vertexBufferResourceDesc.SampleDesc.Count = 1;
	vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device_->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));

	return resource;
}

ComPtr<ID3D12Resource> TextureManager::CreateDepthStencilTextureResource(int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(width);
	resourceDesc.Height = UINT(height);
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));

	return resource;
}

DirectX::ScratchImage TextureManager::LoadTexture(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtils::ConvertString(filePath);

	HRESULT hr = DirectX::LoadFromWICFile(
		filePathW.c_str(),
		DirectX::WIC_FLAGS_DEFAULT_SRGB,
		nullptr,
		image
	);

	if (FAILED(hr)) {
		Logger::Log(std::format("Failed LoadFromWICFile : {}\n", filePath));
	}
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

	if (FAILED(hr)) {
		Logger::Log(std::format("Failed GenerateMipMaps : {}\n", filePath));
	}
	assert(SUCCEEDED(hr));

	return mipImage;
}

void TextureManager::LoadTextureResourceFromFile(TextureID id, const std::string& filePath) {
	if (!ValidateTextureID(id) || textures_.count(id) > 0) {
		Logger::Log(std::format("TextureManager: Invalid or already loaded texture ID {}\n", static_cast<uint32_t>(id)));
		return;
	}

	DirectX::ScratchImage mipImages = LoadTexture(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	TextureResource texRes;
	texRes.resource = CreateTextureResource(metadata);
	texRes.intermediateResource = UploadTextureData(texRes.resource, mipImages);
	texRes.metadata = metadata;
	texRes.isDepthStencil = false;

	textures_[id] = texRes;

	Logger::Log(std::format("TextureManager: Loaded texture ID {} from {}\n", static_cast<uint32_t>(id), filePath));
}

void TextureManager::CreateDepthStencilBuffer(TextureID id, int32_t width, int32_t height) {
	if (!ValidateTextureID(id)) {
		Logger::Log(std::format("TextureManager: Invalid depth stencil ID {}\n", static_cast<uint32_t>(id)));
		return;
	}

	TextureResource depthRes;
	depthRes.resource = CreateDepthStencilTextureResource(width, height);
	depthRes.isDepthStencil = true;
	depthRes.metadata.width = width;
	depthRes.metadata.height = height;
	depthRes.metadata.format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	textures_[id] = depthRes;

	Logger::Log(std::format("TextureManager: Created depth stencil buffer {}x{}\n", width, height));
}

ComPtr<ID3D12Resource> TextureManager::UploadTextureData(
	ComPtr<ID3D12Resource> textureResource,
	const DirectX::ScratchImage& mipImages
) {
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(
		device_,
		mipImages.GetImages(),
		mipImages.GetImageCount(),
		mipImages.GetMetadata(),
		subresources
	);

	uint64_t intermediateSize = GetRequiredIntermediateSize(
		textureResource.Get(),
		0,
		UINT(subresources.size())
	);

	ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);
	UpdateSubresources(
		commandList_,
		textureResource.Get(),
		intermediateResource.Get(),
		0,
		0,
		UINT(subresources.size()),
		subresources.data()
	);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = textureResource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList_->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

void TextureManager::CreateShaderResourceView(TextureID id, DescriptorHeaps* heaps) {
	if (!ValidateTextureID(id) || textures_.count(id) == 0) {
		Logger::Log(std::format("TextureManager: Cannot create SRV for texture ID {}\n", static_cast<uint32_t>(id)));
		return;
	}

	TextureResource& texRes = textures_[id];
	if (texRes.isDepthStencil) {
		Logger::Log("TextureManager: Cannot create SRV for depth stencil\n");
		return;
	}

	if (!heaps) {
		Logger::Log("TextureManager: DescriptorHeaps is nullptr\n");
		return;
	}

	uint32_t descriptorIndex = nextDescriptorIndex_ + 1;
	nextDescriptorIndex_++;
	texRes.descriptorHeapIndex = descriptorIndex;

	// DescriptorHeaps::CreateTextureSRV を呼び出し（戻り値付き）
	auto handles = heaps->CreateTextureSRV_new(
		device_,
		texRes.resource.Get(),
		texRes.metadata.format,
		static_cast<UINT>(texRes.metadata.mipLevels),
		descriptorIndex
	);

	// ハンドルを TextureResource に保存
	texRes.srvCpuHandle = handles.cpuHandle;
	texRes.srvGpuHandle = handles.gpuHandle;

	Logger::Log(std::format("TextureManager: Created SRV for texture ID {} at heap index {}\n",
		static_cast<uint32_t>(id), descriptorIndex));
}

void TextureManager::CreateDepthStencilView(TextureID id, DescriptorHeaps* heaps) {
	if (!ValidateTextureID(id) || textures_.count(id) == 0) {
		Logger::Log(std::format("TextureManager: Cannot create DSV for texture ID {}\n", static_cast<uint32_t>(id)));
		return;
	}

	TextureResource& texRes = textures_[id];
	if (!texRes.isDepthStencil) {
		Logger::Log("TextureManager: Texture is not a depth stencil\n");
		return;
	}

	if (!heaps) {
		Logger::Log("TextureManager: DescriptorHeaps is nullptr\n");
		return;
	}

	texRes.descriptorHeapIndex = 0;
	heaps->CreateDSV(device_, texRes.resource.Get());

	Logger::Log("TextureManager: Created DSV for depth stencil\n");
}

ComPtr<ID3D12Resource> TextureManager::GetResource(TextureID id) const {
	if (!ValidateTextureID(id) || textures_.count(id) == 0) {
		Logger::Log(std::format("TextureManager: Texture ID {} not found\n", static_cast<uint32_t>(id)));
		return nullptr;
	}
	return textures_.at(id).resource;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvGpuHandle(TextureID id) const {
	if (!ValidateTextureID(id) || textures_.count(id) == 0) {
		Logger::Log(std::format("TextureManager: Texture ID {} not found\n", static_cast<uint32_t>(id)));
		return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	}
	return textures_.at(id).srvGpuHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetSrvCpuHandle(TextureID id) const {
	if (!ValidateTextureID(id) || textures_.count(id) == 0) {
		Logger::Log(std::format("TextureManager: Texture ID {} not found\n", static_cast<uint32_t>(id)));
		return D3D12_CPU_DESCRIPTOR_HANDLE{ 0 };
	}
	return textures_.at(id).srvCpuHandle;
}

uint32_t TextureManager::GetDescriptorHeapIndex(TextureID id) const {
	if (!ValidateTextureID(id) || textures_.count(id) == 0) {
		return UINT32_MAX;
	}
	return textures_.at(id).descriptorHeapIndex;
}

bool TextureManager::IsLoaded(TextureID id) const {
	return ValidateTextureID(id) && textures_.count(id) > 0;
}

void TextureManager::Finalize() {
	for (auto& [id, texRes] : textures_) {
		if (texRes.resource) {
			texRes.resource.Reset();
		}
		if (texRes.intermediateResource) {
			texRes.intermediateResource.Reset();
		}
	}
	textures_.clear();
	Logger::Log("TextureManager: Finalized\n");
}

bool TextureManager::ValidateTextureID(TextureID id) const {
	return static_cast<uint32_t>(id) < static_cast<uint32_t>(TextureID::Count);
}

void TextureManager::InitializeDepthStencil(int32_t width, int32_t height, DescriptorHeaps* heaps) {
	depthStencilResource_ = CreateDepthStencilTextureResource(width, height);
	heaps->CreateDSV(device_, depthStencilResource_.Get());
}