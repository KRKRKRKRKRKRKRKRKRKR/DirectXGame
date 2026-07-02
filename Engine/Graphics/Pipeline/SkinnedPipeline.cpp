#include "SkinnedPipeline.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../Utils/Logger.h"
#include <cassert>

void SkinnedPipeline::Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler, bool enableDepth) {
	device_ = device;
	shaderCompiler_ = shaderCompiler;
	enableDepth_ = enableDepth;
	CreateDescriptorRange();
	CreateStaticSamplers();
	CreatePSO();
}

void SkinnedPipeline::CreateDescriptorRange() {
	// [0] t0: テクスチャ（PS）
	descriptorRange_[0].BaseShaderRegister = 0;
	descriptorRange_[0].NumDescriptors = 1;
	descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// [1] t1: WVP配列（VS）
	descriptorRange_[1].BaseShaderRegister = 1;
	descriptorRange_[1].NumDescriptors = 1;
	descriptorRange_[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// [2] t2: 色配列（VS）
	descriptorRange_[2].BaseShaderRegister = 2;
	descriptorRange_[2].NumDescriptors = 1;
	descriptorRange_[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// [3] t3: ボーン行列パレット（VS）
	descriptorRange_[3].BaseShaderRegister = 3;
	descriptorRange_[3].NumDescriptors = 1;
	descriptorRange_[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void SkinnedPipeline::CreateStaticSamplers() {
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers_[0].ShaderRegister = 0;
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
}

void SkinnedPipeline::CreatePSO() {
	CreateRootSignature();
	InputLayout();
	RasterizerState();
	VertexShader();
	PixelShader();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc_;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCount); ++i) {
		BlendMode blendMode = static_cast<BlendMode>(i);
		BlendState(blendMode);
		graphicsPipelineStateDesc.BlendState = blendDesc_;

		DepthStencilState(blendMode);
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc_;

		HRESULT hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates_[i]));
		if (FAILED(hr)) {
			Logger::Log("Failed CreateGraphicsPipelineState (SkinnedPipeline BlendMode)\n");
		}
		assert(SUCCEEDED(hr));
	}
}

void SkinnedPipeline::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[7] = {};

	// [0] t0: テクスチャ（PS）
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange_[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// [1] t1: WVP配列（VS）
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange_[1];
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	// [2] t2: 色配列（VS）
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange_[2];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	// [3] b0: ライト（PS）
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 0;
	rootParameters[3].Descriptor.RegisterSpace = 0;

	// [4] b1: インスタンスオフセット（VS）
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[4].Constants.ShaderRegister = 1;
	rootParameters[4].Constants.RegisterSpace = 0;
	rootParameters[4].Constants.Num32BitValues = 1;

	// [5] b2: ブレンド/αTest情報（PS）。x=blendMode, y=blendStrength, z=enableAlphaTest, w=alphaThreshold
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Constants.ShaderRegister = 2;
	rootParameters[5].Constants.RegisterSpace = 0;
	rootParameters[5].Constants.Num32BitValues = 4;

	// [6] t3: ボーン行列パレット（VS）- スキニング専用
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[6].DescriptorTable.pDescriptorRanges = &descriptorRange_[3];
	rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	descriptionRootSignature.pStaticSamplers = staticSamplers_;
	descriptionRootSignature.NumStaticSamplers = STATIC_SAMPLER_COUNT;

	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		if (errorBlob_) {
			Logger::Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		} else {
			Logger::Log("SkinnedPipeline: D3D12SerializeRootSignature failed (no error blob)\n");
		}
	}
	assert(SUCCEEDED(hr));

	hr = device_->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateRootSignature (SkinnedPipeline)\n");
	}
	assert(SUCCEEDED(hr));
}

void SkinnedPipeline::InputLayout() {
	inputElementDescs_[0].SemanticName = "POSITION";
	inputElementDescs_[0].SemanticIndex = 0;
	inputElementDescs_[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs_[1].SemanticName = "TEXCOORD";
	inputElementDescs_[1].SemanticIndex = 0;
	inputElementDescs_[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs_[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs_[2].SemanticName = "NORMAL";
	inputElementDescs_[2].SemanticIndex = 0;
	inputElementDescs_[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs_[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs_[3].SemanticName = "BLENDINDICES";
	inputElementDescs_[3].SemanticIndex = 0;
	inputElementDescs_[3].Format = DXGI_FORMAT_R32G32B32A32_UINT;
	inputElementDescs_[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs_[4].SemanticName = "BLENDWEIGHT";
	inputElementDescs_[4].SemanticIndex = 0;
	inputElementDescs_[4].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs_[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputLayoutDesc_.pInputElementDescs = inputElementDescs_;
	inputLayoutDesc_.NumElements = _countof(inputElementDescs_);
}

// BlendState/RasterizerState/DepthStencilStateはPipelineクラスと同一ロジック
void SkinnedPipeline::BlendState(BlendMode blendMode) {
	blendDesc_ = {};
	D3D12_RENDER_TARGET_BLEND_DESC& rt = blendDesc_.RenderTarget[0];
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.DestBlendAlpha = D3D12_BLEND_ZERO;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	switch (blendMode) {
	case BlendMode::kNone:
	default:
		rt.BlendEnable = FALSE;
		break;

	case BlendMode::kNormal:
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_BLEND_FACTOR;
		rt.DestBlend = D3D12_BLEND_INV_BLEND_FACTOR;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kAdd:
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_BLEND_FACTOR;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kSubtract:
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_BLEND_FACTOR;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		break;

	case BlendMode::kMultiply:
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_ZERO;
		rt.DestBlend = D3D12_BLEND_SRC_COLOR;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kScreen:
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_ONE;
		rt.DestBlend = D3D12_BLEND_INV_SRC_COLOR;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;
	}
}

void SkinnedPipeline::RasterizerState() {
	rasterizerDesc_ = {};
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc_.DepthClipEnable = TRUE;
}

void SkinnedPipeline::VertexShader() {
	vertexShaderBlob_.Attach(shaderCompiler_->CompileShader(L"HLSL/SkinnedObject3d.VS.hlsl", L"vs_6_0"));
	assert(vertexShaderBlob_ != nullptr);
}

void SkinnedPipeline::PixelShader() {
	// PSはObject3d.PS.hlslをそのまま流用（出力構造体が共通のため）
	pixelShaderBlob_.Attach(shaderCompiler_->CompileShader(L"HLSL/Object3D.PS.hlsl", L"ps_6_0"));
	assert(pixelShaderBlob_ != nullptr);
}

void SkinnedPipeline::DepthStencilState(BlendMode blendMode) {
	depthStencilDesc_ = {};
	depthStencilDesc_.DepthEnable = enableDepth_ ? TRUE : FALSE;
	bool writeDepth = enableDepth_ && (blendMode == BlendMode::kNone);
	depthStencilDesc_.DepthWriteMask = writeDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}
