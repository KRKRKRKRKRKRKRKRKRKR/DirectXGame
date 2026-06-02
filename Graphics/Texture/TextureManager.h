#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "../../Externals/DirectXTex/DirectXTex.h"
#include <map>
#include <string>

using Microsoft::WRL::ComPtr;

enum class TextureID : uint32_t {
	DepthStencil,
	Texture1,
	Texture2,
	Texture3,
	Count
};

struct TextureResource {
	ComPtr<ID3D12Resource> resource;
	ComPtr<ID3D12Resource> intermediateResource;
	DirectX::TexMetadata metadata;
	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle{ 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle{ 0 };
	uint32_t descriptorHeapIndex = UINT32_MAX;
	bool isDepthStencil = false;
};

class DescriptorHeaps;  // Forward declaration

class TextureManager {
public:
	TextureManager() = default;
	~TextureManager() = default;

	// === リソース生成 ===
	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
	ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);

	// === テクスチャロード・初期化 ===
	DirectX::ScratchImage LoadTexture(const std::string& filePath);
	void LoadTextureResourceFromFile(TextureID id, const std::string& filePath);
	void CreateDepthStencilBuffer(TextureID id, int32_t width, int32_t height);

	// === GPU転送 ===
	ComPtr<ID3D12Resource> UploadTextureData(
		ComPtr<ID3D12Resource> textureResource,
		const DirectX::ScratchImage& mipImages
	);

	// === ビュー作成（DescriptorHeaps 経由） ===
	void CreateShaderResourceView(TextureID id, DescriptorHeaps* heaps);
	void CreateDepthStencilView(TextureID id, DescriptorHeaps* heaps);

	// === リソースアクセス ===
	ComPtr<ID3D12Resource> GetResource(TextureID id) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(TextureID id) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle(TextureID id) const;
	uint32_t GetDescriptorHeapIndex(TextureID id) const;
	bool IsLoaded(TextureID id) const;

	// === クリーンアップ ===
	void Finalize();

	// === デバイス設定 ===
	void SetDevice(ID3D12Device* device) { device_ = device; }
	void SetCommandList(ID3D12GraphicsCommandList* cmdList) { commandList_ = cmdList; }

private:
	std::map<TextureID, TextureResource> textures_;
	ID3D12Device* device_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;
	uint32_t nextDescriptorIndex_ = 0;

	bool ValidateTextureID(TextureID id) const;
};