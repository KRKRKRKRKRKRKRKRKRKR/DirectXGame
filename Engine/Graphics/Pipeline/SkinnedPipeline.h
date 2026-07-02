#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include "BlendMode.h"
using Microsoft::WRL::ComPtr;

class ShaderCompiler;

// スキニングモデル専用のPipeline。Pipelineクラスと構造は同じだが、
// InputLayoutにBLENDINDICES/BLENDWEIGHTを追加し、ルートシグネチャに
// ボーン行列パレット（t3, StructuredBuffer）を追加した専用のVS/ルートシグネチャを使う。
// 静的形状（Cube/Sphere/Triangle等）が使う既存Pipelineには一切影響しない。
class SkinnedPipeline {
public:
	SkinnedPipeline() = default;
	~SkinnedPipeline() = default;

	void Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler, bool enableDepth = true);

	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
	ID3D12PipelineState* GetPipelineState(BlendMode blendMode = BlendMode::kNone) const {
		return graphicsPipelineStates_[static_cast<size_t>(blendMode)].Get();
	}

private:
	void CreateDescriptorRange();
	void CreateStaticSamplers();

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
	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 4; // t0テクスチャ, t1 WVP, t2 色, t3 ボーン行列パレット
	static constexpr UINT STATIC_SAMPLER_COUNT = 1;
	D3D12_DESCRIPTOR_RANGE descriptorRange_[DESCRIPTOR_RANGE_COUNT] = {};
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[STATIC_SAMPLER_COUNT] = {};

	bool enableDepth_ = true;
	ComPtr<ID3DBlob> signatureBlob_ = nullptr;
	ComPtr<ID3DBlob> errorBlob_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	ComPtr<ID3D12PipelineState> graphicsPipelineStates_[static_cast<size_t>(BlendMode::kCount)];
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[5] = {}; // POSITION/TEXCOORD/NORMAL/BLENDINDICES/BLENDWEIGHT
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_ = {};
	D3D12_BLEND_DESC blendDesc_ = {};
	D3D12_RASTERIZER_DESC rasterizerDesc_ = {};
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_ = {};

	ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
	ComPtr<IDxcBlob> pixelShaderBlob_ = nullptr;
};
