#pragma once
#include <Windows.h>
#include <dxcapi.h>

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

class ShaderCompiler;

class LinePipeline {
public:
	LinePipeline() = default;
	~LinePipeline() = default;

	void Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler);

	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
	ID3D12PipelineState* GetPipelineState() const { return graphicsPipelineState_.Get(); }

private:
	void CreateRootSignature();
	void InputLayout();
	void BlendState();
	void RasterizerState();
	void VertexShader();
	void PixelShader();
	void DepthStencilState();
	void CreatePSO();

	// デスクリプタレンジ・サンプラーはLineには不要なので省略

	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[1] = {};  // POSITIONのみ
	D3D12_INPUT_LAYOUT_DESC  inputLayoutDesc_{};
	D3D12_BLEND_DESC         blendDesc_{};
	D3D12_RASTERIZER_DESC    rasterizerDesc_{};
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_{};

	ComPtr<IDxcBlob> vertexShaderBlob_;
	ComPtr<IDxcBlob> pixelShaderBlob_;
	ComPtr<ID3DBlob> signatureBlob_;
	ComPtr<ID3DBlob> errorBlob_;

	ComPtr<ID3D12RootSignature>   rootSignature_;
	ComPtr<ID3D12PipelineState>   graphicsPipelineState_;

	ID3D12Device* device_ = nullptr;
	ShaderCompiler* shaderCompiler_ = nullptr;
};