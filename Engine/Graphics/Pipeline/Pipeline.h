#pragma once
#include "PipelineBase.h"

// 静的形状（Cube/Sphere/Triangle/Sprite/非スキニングModel）用のPipeline。
// 共通ロジックはPipelineBaseに実装済みで、ここでは頂点フォーマット・ルートシグネチャ・
// VertexShaderという「静的メッシュ固有」の部分だけを実装する。
class Pipeline : public PipelineBase {
public:
	Pipeline() = default;
	~Pipeline() override = default;

private:
	void CreateDescriptorRange() override;
	void CreateRootSignature() override;
	void InputLayout() override;
	void VertexShader() override;

	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 3;
	D3D12_DESCRIPTOR_RANGE descriptorRange_[DESCRIPTOR_RANGE_COUNT] = {};
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[3] = {};
};
