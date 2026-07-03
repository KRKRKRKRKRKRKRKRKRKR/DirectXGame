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

	// SRVヒープ全体の容量。index 0〜(TextureManager::kMaxTextureCount-1)がテクスチャ専用帯、
	// 残りがAllocateSRVIndexの払い出し対象
	static constexpr uint32_t kSrvHeapCapacity = 1024;

	void Initialize(ID3D12Device* device);
	void Finalize();

	void CreateRTV(ID3D12Device* device, ID3D12Resource* resource0, ID3D12Resource* resource1);
	void CreateDSV(ID3D12Device* device, ID3D12Resource* depthStencilResource);
	void CreateTextureSRV(ID3D12Device* device, ID3D12Resource* textureResource, DXGI_FORMAT format, UINT mipLevels, uint32_t heapIndex);

	// 任意のリソース・フォーマットで、RTV/DSVヒープの指定indexへ1枚だけビューを作る汎用版。
	// CreateRTV/CreateDSV（スワップチェイン専用の固定2枚/1枚版）はこれの薄いラッパー
	void CreateRTVAt(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, uint32_t index);
	void CreateDSVAt(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, uint32_t index);

	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetRTVDescriptorHeap() const { return rtvDescriptorHeap_.Get(); }
	ID3D12DescriptorHeap* GetDSVDescriptorHeap() const { return dsvDescriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index) const { return rtvHandles_[index]; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandles_[0]; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(uint32_t index) const { return dsvHandles_[index]; }

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

	// SRVヒープインデックスの一元管理用アロケータ。Triangle/Cube/Sphere/Sprite/Model等の
	// WVP・色・ボーン行列パレット用SRVは、各クラスがマジックナンバーを持つ代わりにここから
	// 未使用の連続indexをcount個受け取る。テクスチャ用index（TextureManagerが独自に
	// handleをそのままheapIndexとして使う自己完結の帯）とは別に、この払い出しは
	// nextSrvAllocIndex_ から開始することで衝突を避ける（Initialize内で開始位置を設定）
	uint32_t AllocateSRVIndex(uint32_t count = 1);

private:
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	void SetDescriptorSizes(ID3D12Device* device);

	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;
	// index 0/1: スワップチェインのバックバッファ, index 2: 鏡の反射用オフスクリーンRTV
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[3] = {};
	// index 0: メイン画面用, index 1: 鏡の反射用オフスクリーン深度
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandles_[2] = {};
	std::map<uint32_t, SrvHandle> textureSrvHandles_;
	uint32_t descriptorSizeSRV_ = 0;
	uint32_t descriptorSizeRTV_ = 0;
	uint32_t descriptorSizeDSV_ = 0;

	// AllocateSRVIndexが次に払い出すindex。テクスチャ用の帯（TextureManager::kMaxTextureCount未満）
	// と衝突しないよう、Initialize内でkMaxTextureCountから開始する
	uint32_t nextSrvAllocIndex_ = 0;
};