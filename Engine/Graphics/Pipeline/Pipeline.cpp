#include "Pipeline.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../Utils/Logger.h"
#include <cassert>

//Piplineの初期化
void Pipeline::Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler) {
	device_ = device;
    shaderCompiler_ = shaderCompiler;
	CreateDescriptorRange();
	CreateStaticSamplers();
	CreatePSO();
}

#pragma region Piplineの設定
//パイプラインの設定
void Pipeline::CreateDescriptorRange() {
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
}

//サンプラーの設定
void Pipeline::CreateStaticSamplers() {
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers_[0].ShaderRegister = 0;
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
}
#pragma endregion

#pragma region PSOの設定
//PSOの作成
void Pipeline::CreatePSO() {
	CreateRootSignature();
	InputLayout();
	BlendState();
	RasterizerState();
	VertexShader();
	PixelShader();
	DepthStencilState();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc_;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc_;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc_;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	HRESULT hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateGraphicsPipelineState\n");
	}
	assert(SUCCEEDED(hr));
}

#pragma region PSOの内部関数
//PSOのルートシグネチャの作成
void Pipeline::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};

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

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	descriptionRootSignature.pStaticSamplers = staticSamplers_;
	descriptionRootSignature.NumStaticSamplers = STATIC_SAMPLER_COUNT;

	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		Logger::Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
	}
	assert(SUCCEEDED(hr));

	hr = device_->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	if (FAILED(hr)) {
		Logger::Log("Failed CreateRootSignature\n");
	}
	assert(SUCCEEDED(hr));
}

//PSOのInputLayoutの設定
void Pipeline::InputLayout() {
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
	inputLayoutDesc_.pInputElementDescs = inputElementDescs_;
	inputLayoutDesc_.NumElements = _countof(inputElementDescs_);
}

//PSOのBlendStateの設定
void Pipeline::BlendState() {
	blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

//PSOのRasterizerStateの設定
void Pipeline::RasterizerState() {
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
}

//PSOのVertexShaderの設定
void Pipeline::VertexShader() {
	vertexShaderBlob_.Attach(shaderCompiler_->CompileShader(L"HLSL/Object3D.VS.hlsl", L"vs_6_0"));
	assert(vertexShaderBlob_ != nullptr);
}

//PSOのPixelShaderの設定
void Pipeline::PixelShader() {
	pixelShaderBlob_.Attach(shaderCompiler_->CompileShader(L"HLSL/Object3D.PS.hlsl", L"ps_6_0"));
	assert(pixelShaderBlob_ != nullptr);
}

//PSOのDepthStencilStateの設定
void Pipeline::DepthStencilState() {
	depthStencilDesc_.DepthEnable = TRUE;
	depthStencilDesc_.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}
#pragma endregion
#pragma	endregion