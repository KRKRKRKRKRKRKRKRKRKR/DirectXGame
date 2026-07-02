#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "../../../Externals/DirectXTex/DirectXTex.h"
#include <unordered_map>
#include <string>
#include <cstdint>

using Microsoft::WRL::ComPtr;

// テクスチャを識別するハンドル。0 = 白テクスチャ（テクスチャなし扱い）
using TextureHandle = uint32_t;
static constexpr TextureHandle kTextureNone = 0;

// SRVヒープのうちテクスチャ専用に確保する枚数（index 0〜kMaxTextureCount-1）。
// これを超える分は他オブジェクト（Triangle/Cube/Sphere/Sprite/Model等、index kMaxTextureCount〜）の
// SRVスロットと衝突するため、この帯を十分に大きく確保している（DescriptorHeaps::Initialize参照）
static constexpr uint32_t kMaxTextureCount = 500;

struct TextureResource {
	ComPtr<ID3D12Resource> resource;
	ComPtr<ID3D12Resource> intermediateResource;
	DirectX::TexMetadata metadata;
	D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle{ 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle{ 0 };
	uint32_t descriptorHeapIndex = UINT32_MAX;
};

class DescriptorHeaps;

class TextureManager {
public:
	TextureManager() = default;
	~TextureManager() = default;

	// === デバイス設定（Initialize前に呼ぶ） ===
	void SetDevice(ID3D12Device* device) { device_ = device; }
	void SetCommandList(ID3D12GraphicsCommandList* cmdList) { commandList_ = cmdList; }

	// エンジン起動時に白テクスチャ（handle=0）を登録する
	void InitializeDefaultTexture(DescriptorHeaps* heaps);

	// ファイルからテクスチャを読み込んでハンドルを返す
	TextureHandle Load(const std::string& filePath, DescriptorHeaps* heaps);

	// 深度ステンシルバッファの初期化（エンジン内部用）
	void InitializeDepthStencil(int32_t width, int32_t height, DescriptorHeaps* heaps);

	// === リソースアクセス ===
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(TextureHandle handle) const;
	ID3D12Resource* GetDepthStencilResource() const { return depthStencilResource_.Get(); }

	bool IsLoaded(TextureHandle handle) const;

	// === クリーンアップ ===
	void Finalize();

private:
	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);
	DirectX::ScratchImage LoadFromFile(const std::string& filePath);
	ComPtr<ID3D12Resource> UploadTextureData(ComPtr<ID3D12Resource> textureResource, const DirectX::ScratchImage& mipImages);
	TextureHandle RegisterTexture(ComPtr<ID3D12Resource> resource, const DirectX::TexMetadata& metadata, ComPtr<ID3D12Resource> intermediate, DescriptorHeaps* heaps);

	std::unordered_map<TextureHandle, TextureResource> textures_;
	std::unordered_map<std::string, TextureHandle> pathToHandle_;  // 同じパスの二重ロード防止
	ID3D12Device* device_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;
	uint32_t nextHandle_ = 0;
	ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;
};