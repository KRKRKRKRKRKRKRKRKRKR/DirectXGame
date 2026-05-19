#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>

class ShaderCompiler;

using Microsoft::WRL::ComPtr;
class PipelineStateManager {
public:
	PipelineStateManager() = default;
	~PipelineStateManager() = default;
	void Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler);

private:
	void CreateRootSignature(ID3D12Device* device);
	void CreateGraphicsPipelineState(ID3D12Device* device,ShaderCompiler* shaderCompiler);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;

	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 1;
	static constexpr UINT STATIC_SAMPLER_COUNT = 1;
};