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

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using Microsoft::WRL::ComPtr;

class DirectXManager {
public:
	DirectXManager() = default;
	~DirectXManager();

	//===== ライフサイクル =====
	void Initialize(HWND hwnd, int32_t width, int32_t height);
	void Finalize();

	// ===== フレーム管理 =====
	void BeginFrame();
	void EndFrame();

	// ===== 描画関連 =====
	void DrawTriangleRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
	void DrawSpriteRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
	void CreateDrawSphereResource(const SphereData& sphereData, const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);

	// =====  ゲッター =====
	ID3D12Device* GetDevice() const { return device_.Get(); }
	IDXGIFactory7* GetFactory() const { return dxgiFactory_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return srvDescriptorHeap_.Get(); }
	const D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() const { return descriptorRange_; }
	UINT GetDescriptorRangeCount() const { return DESCRIPTOR_RANGE_COUNT; }
	const D3D12_STATIC_SAMPLER_DESC* GetStaticSamplers() const { return staticSamplers_; }
	UINT GetStaticSamplerCount() const { return STATIC_SAMPLER_COUNT; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvHandle() const { return textureSrvHandle_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvHandle_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return rtvHandles_[backBufferIndex_]; }
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

	// ===== Descriptor Heapsの作成 =====
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	void CreateRTVDescriptorHeap();
	void CreateSRVDescriptorHeap();
	void CreateDSVDescriptorHeap();

	// ===== Render Target Viewsの作成 =====
	void CreateRTV();

	// ===== 同期オブジェクトの作成 =====
	void CreateFence();
	void WaitForGPUCompletion();

	// ===== リソース管理 =====
	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
	ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
	ComPtr<ID3D12Resource> UploadTextureData(ComPtr<ID3D12Resource> textureResource, const DirectX::ScratchImage& mipImages);
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);

	// ===== テクスチャロード =====
	DirectX::ScratchImage LoadTexture(const std::string& filePath);
	void LoadTextureResource(const std::string& filePath);
	void CreateShaderResourceView(const DirectX::TexMetadata& metadata, ComPtr<ID3D12Resource> textureResource);
	void DepthShaderResourceView();

	// ===== 状態遷移バリア =====
	void BeginTransitionBarrier();
	void EndTransitionBarrier();

	// =====　パイプライン設定 =====
	void CreateDescriptorRange();
	void CreateStaticSamplers();

	// ===== DXC & シェーダーコンパイル =====
	void InitializeDXC();
	IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile);
	void LoadHLSLFile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource);
	void ExecuteCompile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource, IDxcResult*& shaderResult);
	void LogCompileErrors(IDxcResult* shaderResult);
	IDxcBlob* GetShaderBlob(const std::wstring& filePath, const wchar_t* profile, IDxcResult* shaderResult);

	// ===== PSO作成 =====
	void CreatePSO();
	void CreateRootSignature();
	void InputLayout();
	void BlendState();
	void RasterizerState();
	void VertexShader();
	void PixelShader();
	void DepthStencilState();

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
	ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2] = {};
	ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;

	// ===== テクスチャリソースの作成 =====
	ComPtr<ID3D12Resource> textureResource_ = nullptr;
	ComPtr<ID3D12Resource> intermediateResource_ = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle_ = {};
	ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_ = {};

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

	// ===== DXC 関連 =====
	ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	DxcBuffer shaderSourceBuffer_;

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
	Matrix4x4* wvpData_ = nullptr;
	Matrix4x4* wvpDataSphere_ = nullptr;
	ComPtr<ID3D12Resource> transformationMatrixResourceSprite_ = nullptr;
	Matrix4x4* transformationMatrixDataSprite_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite_{};
	uint32_t sphereVertexCount_ = 0;

};