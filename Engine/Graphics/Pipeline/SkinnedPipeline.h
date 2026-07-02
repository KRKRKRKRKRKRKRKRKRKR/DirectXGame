#pragma once
#include "PipelineBase.h"

// スキニングモデル専用のPipeline。共通ロジックはPipelineBaseに実装済みで、ここでは
// InputLayoutにBLENDINDICES/BLENDWEIGHTを追加し、ルートシグネチャにボーン行列パレット
// （t3, StructuredBuffer）を追加した専用のVS/ルートシグネチャだけを実装する。
// 静的形状（Cube/Sphere/Triangle等）が使う既存Pipelineには一切影響しない。
class SkinnedPipeline : public PipelineBase {
public:
	SkinnedPipeline() = default;
	~SkinnedPipeline() override = default;

private:
	void CreateDescriptorRange() override;
	void CreateRootSignature() override;
	void InputLayout() override;
	void VertexShader() override;

	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 4; // t0テクスチャ, t1 WVP, t2 色, t3 ボーン行列パレット
	D3D12_DESCRIPTOR_RANGE descriptorRange_[DESCRIPTOR_RANGE_COUNT] = {};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[5] = {}; // POSITION/TEXCOORD/NORMAL/BLENDINDICES/BLENDWEIGHT
};
