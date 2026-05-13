#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <string>
#include <cstdint>
#include <wrl.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#include "../../DirectXManager.h"
#include "../../../Math/MathTypes.h"

using Microsoft::WRL::ComPtr;

class PrimitiveRenderer {
public:
	PrimitiveRenderer() = default;
	~PrimitiveRenderer();
	void Initialize(DirectXManager* dx, int32_t windowWidth, int32_t windowHeight);
	void DrawTriangleRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
	void Finalize();

private:


	void InitializeDXC();
	IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile);
	void LoadHLSLFile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource);
	void ExecuteCompile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource, IDxcResult*& shaderResult);
	void LogCompileErrors(IDxcResult* shaderResult);
	IDxcBlob* GetShaderBlob(const std::wstring& filePath, const wchar_t* profile, IDxcResult* shaderResult);
	ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	DxcBuffer shaderSourceBuffer_;
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[2] = {};
	void CreatePSO(DirectXManager* dx);
	void CreateRootSignature(DirectXManager* dx);
	void InputLayout();
	void BlendState();
	void RasterizerState();
	void VertexShader();
	void PixelShader();
	void DepthStencilState();

	ComPtr<ID3DBlob> signatureBlob_ = nullptr;
	ComPtr<ID3DBlob> errorBlob_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
	ComPtr<IDxcBlob> pixelShaderBlob_ = nullptr;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_ = {};
	D3D12_BLEND_DESC blendDesc_ = {};
	D3D12_RASTERIZER_DESC rasterizerDesc_ = {};
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_ = {};

	ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
	ID3D12Resource* CreateBufferResource(DirectXManager* dx, size_t sizeInBytes);
	void CreateVertexResource(DirectXManager* dx);
	void CreateMaterialResource(DirectXManager* dx);
	void CreateVertexBufferView();
	void WriteVertexResource();
	void ViewportScissorRect(int32_t width, int32_t height);
	void CreateTransformationMatrix(DirectXManager* dx);
	void SetPipelineCommands();
	void RecordDrawCommands();

	ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	ComPtr<ID3D12Resource> materialResource_ = nullptr;
	ComPtr<ID3D12Resource> wvpResource_ = nullptr;
	Matrix4x4* wvpData_ = nullptr;
	DirectXManager* dx_ = nullptr;
	ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	int32_t windowWidth_ = 0;
	int32_t windowHeight_ = 0;


};

