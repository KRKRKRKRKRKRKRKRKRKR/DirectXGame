#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include "BlendMode.h"
using Microsoft::WRL::ComPtr;

class ShaderCompiler;
class Pipeline {
public:
	Pipeline() = default;
	~Pipeline() = default;

	//Piplineの初期化
	void Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler, bool enableDepth = true);

	//ゲッター
	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
	// blendMode に対応するPSOを返す（BlendStateだけが異なるPSOを事前に全種類作っておき、描画時に選ぶ）
	ID3D12PipelineState* GetPipelineState(BlendMode blendMode = BlendMode::kNone) const {
		return graphicsPipelineStates_[static_cast<size_t>(blendMode)].Get();
	}

private:

	void CreateDescriptorRange();
	void CreateStaticSamplers();

	//PSO作成
	void CreatePSO();
	void CreateRootSignature();
	void InputLayout();
	void BlendState(BlendMode blendMode);
	void RasterizerState();
	void VertexShader();
	void PixelShader();
	void DepthStencilState(BlendMode blendMode);

	ComPtr<ID3D12Device> device_;
	ShaderCompiler* shaderCompiler_ = nullptr;
	D3D12_RESOURCE_BARRIER barrier_ = {};
	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 3;
	static constexpr UINT STATIC_SAMPLER_COUNT = 1;
	D3D12_DESCRIPTOR_RANGE descriptorRange_[DESCRIPTOR_RANGE_COUNT] = {};
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[STATIC_SAMPLER_COUNT] = {};

	bool enableDepth_ = true;
	ComPtr<ID3DBlob> signatureBlob_ = nullptr;
	ComPtr<ID3DBlob> errorBlob_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	// ブレンドモードごとに1つずつ、計 BlendMode::kCount 個のPSOを保持する
	ComPtr<ID3D12PipelineState> graphicsPipelineStates_[static_cast<size_t>(BlendMode::kCount)];
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[3] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_ = {};
	D3D12_BLEND_DESC blendDesc_ = {};
	D3D12_RASTERIZER_DESC rasterizerDesc_ = {};
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_ = {};

	ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
	ComPtr<IDxcBlob> pixelShaderBlob_ = nullptr;
};