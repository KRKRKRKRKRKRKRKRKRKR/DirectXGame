#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cstdint>
#include <string>
#include <wrl.h>
#include "../Externals/DirectXTex/DirectXTex.h"
#include "../Externals/DirectXTex/d3dx12.h"
#include "../Math/MathTypes.h"

#include "../ShaderCompiler.h"
#include "../Pipline.h"
#include "../DescriptorHeaps.h"
#include "../TextureManager.h"
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
    void DrawTriangleRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, uint32_t wvpIndex = 0);
	void DrawSpriteRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
 void CreateDrawSphereResource(const SphereData& sphereData, const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, bool changeTexture, uint32_t wvpIndex = 1);

	uint32_t AllocateWvpIndex();
	void SetWvpMatrix(uint32_t index, const Matrix4x4& matrix);
	D3D12_GPU_VIRTUAL_ADDRESS GetWvpGpuAddress(uint32_t index) const;

	// =====  ゲッター =====
	ID3D12Device* GetDevice() const { return device_.Get(); }
	IDXGIFactory7* GetFactory() const { return dxgiFactory_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
 ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return descriptorHeaps_.GetSRVDescriptorHeap(); }
	const D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() const { return descriptorRange_; }
	UINT GetDescriptorRangeCount() const { return DESCRIPTOR_RANGE_COUNT; }
	const D3D12_STATIC_SAMPLER_DESC* GetStaticSamplers() const { return staticSamplers_; }
	UINT GetStaticSamplerCount() const { return STATIC_SAMPLER_COUNT; }
   D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvHandle() const { return descriptorHeaps_.GetTextureSrvHandle(); }
 D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return descriptorHeaps_.GetDSVHandle(); }
  D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return descriptorHeaps_.GetRTVHandle(backBufferIndex_); }
	UINT GetBackBufferIndex() const { return backBufferIndex_; }

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
	void CreateVertexResource();
	void CreateMaterialResource();
	void CreateVertexBufferView();
	void WriteVertexResource();
	void ViewportScissorRect(int32_t width, int32_t height);
	void CreateTransformationMatrix();
	void SetPipelineCommands();
	void RecordDrawCommands();

	void CreateVertexSpriteResource();
	void SetVertexSpriteResource();
	void CreateVertexTransformMatrixResource();

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

	// ===== PSO 関連 =====
	ComPtr<ID3DBlob> signatureBlob_ = nullptr;
	ComPtr<ID3DBlob> errorBlob_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
	ComPtr<IDxcBlob> pixelShaderBlob_ = nullptr;
	ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;

	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[2] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_ = {};
	D3D12_BLEND_DESC blendDesc_ = {};
	D3D12_RASTERIZER_DESC rasterizerDesc_ = {};
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_ = {};

	// ===== 描画用リソース =====
	ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	ComPtr<ID3D12Resource> materialResource_ = nullptr;
    ComPtr<ID3D12Resource> wvpResource_ = nullptr;
	ComPtr<ID3D12Resource> vertexResourceSprite_ = nullptr;
	ComPtr<ID3D12Resource> vertexResourceSphere_ = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere_{};
    uint8_t* wvpMappedData_ = nullptr;
	uint32_t wvpStride_ = 0;
	uint32_t wvpAllocatedCount_ = 0;
	static constexpr uint32_t kTriangleWvpIndex = 0;
	static constexpr uint32_t kSphereWvpIndex = 1;
	ComPtr<ID3D12Resource> transformationMatrixResourceSprite_ = nullptr;
	Matrix4x4* transformationMatrixDataSprite_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite_{};
	uint32_t sphereVertexCount_ = 0;
	float sphereGeometryRadius_ = 0.0f;




	ShaderCompiler shaderCompiler_;
	Pipline pipline_;
	TextureManager textureManager_;
};