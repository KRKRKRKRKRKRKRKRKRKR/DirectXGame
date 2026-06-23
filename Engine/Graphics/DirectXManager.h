#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cstdint>
#include <string>
#include <wrl.h>
#include "../../Externals/DirectXTex/DirectXTex.h"
#include "../../Externals/DirectXTex/d3dx12.h"
#include "../../Math/MathTypes.h"
#include "Object/Sprite/Sprite.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Pipeline/Pipeline.h"
#include "Pipeline/LinePipeline.h"
#include "DescriptorHeaps/DescriptorHeaps.h"
#include "Texture/TextureManager.h"
#include "Object/Triangle/Triangle.h"
#include "Object/Line/Line.h"
#include "Object/Sphere/Sphere.h"
#include "ResourceFactory/ResourceFactory.h"
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using Microsoft::WRL::ComPtr;

class DirectXManager {
public:
	DirectXManager() = default;
	~DirectXManager();

	static constexpr uint32_t kMaxWvpCount = 100;

	//===== ライフサイクル =====
	void Initialize(HWND hwnd, int32_t width, int32_t height);
	void Finalize();
	
	// ===== フレーム管理 =====
	void BeginFrame();
	void EndFrame();

	// ===== 描画関連 =====
	void DrawTriangleRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, const Vector4& color, TextureID textureID);
	void DrawSpriteRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
	void DrawLineRender(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& start, const Vector3& end, const Vector4& color);
	void DrawSphereRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, TextureID textureID);

	// =====  ゲッター =====
	ID3D12Device* GetDevice() const { return device_.Get(); }
	IDXGIFactory7* GetFactory() const { return dxgiFactory_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return descriptorHeaps_.GetSRVDescriptorHeap(); }
	const D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() const { return descriptorRange_; }
	UINT GetDescriptorRangeCount() const { return DESCRIPTOR_RANGE_COUNT; }
	const D3D12_STATIC_SAMPLER_DESC* GetStaticSamplers() const { return staticSamplers_; }
	UINT GetStaticSamplerCount() const { return STATIC_SAMPLER_COUNT; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return descriptorHeaps_.GetDSVHandle(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return descriptorHeaps_.GetRTVHandle(backBufferIndex_); }
	UINT GetBackBufferIndex() const { return backBufferIndex_; }
	int32_t GetClientWidth() const { return windowWidth_; }
	int32_t GetClientHeight() const { return windowHeight_; }
	Triangle* GetTriangle() { return triangle_.get(); }
	TextureManager* GetTextureManager() { return &textureManager_; }
	void InitializeGridLines();
	void DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection);

private:
	int32_t windowWidth_ = 0;
	int32_t windowHeight_ = 0;

	// ===== COMの初期化 =====
	void InitializeCOM();
	void FinalizeCOM();

	// ===== Factory & Deviceの作成 =====
	void CreateFactory();
	void SelectAdapter();
	void CreateDevice();

	// ===== コマンドキューとコマンドリストの作成 =====
	void CreateCommandQueue();

	// ===== スワップチェーンの作成 =====
	void CreateSwapChain(HWND hwnd);
	void GetSwapChainResources();

	// ===== 同期オブジェクトの作成 =====
	void CreateFence();
	void WaitForGPUCompletion();


	// ===== 状態遷移バリア =====
	void BeginTransitionBarrier();
	void EndTransitionBarrier();


	// ===== 描画リソース作成 =====
	void ViewportScissorRect(int32_t width, int32_t height);


	// ===== 状態フラグ =====
	bool initialized_ = false;
	bool comInitialized_ = false;
	bool isTextureLoaded_ = false;

	// ===== DXGI & Device Members =====
	ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;
	ComPtr<ID3D12Device> device_ = nullptr;

	// ===== コマンドキューとコマンドリストの作成 =====
	ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;

	// ===== スワップチェーンの作成 =====
	ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	ComPtr<ID3D12Resource> swapChainResources_[2] = { nullptr, nullptr };
	UINT backBufferIndex_ = 0;

	// ===== Descriptor Heapsの作成 =====
	DescriptorHeaps descriptorHeaps_;


	

	void InitializeTexture();





	// ===== 同期オブジェクトの作成 =====
	ComPtr<ID3D12Fence> fence_ = nullptr;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	// ===== パイプラインステートメンバー =====
	D3D12_RESOURCE_BARRIER barrier_ = {};
	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 1;
	D3D12_DESCRIPTOR_RANGE descriptorRange_[DESCRIPTOR_RANGE_COUNT] = {};
	static constexpr UINT STATIC_SAMPLER_COUNT = 1;
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[STATIC_SAMPLER_COUNT] = {};

	// ===== PSO 関連 ====

	// ===== 描画用リソース =====
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};



	ShaderCompiler shaderCompiler_;
	Pipeline pipeline_;
		LinePipeline linePipeline_;
	TextureManager textureManager_;

	std::unique_ptr<Triangle> triangle_;
	uint32_t currentTriangleWvpIndex_ = 0;

	std::unique_ptr<Line> line_;
	uint32_t currentLineWvpIndex_ = 0;

	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Sphere> sphere_;

};