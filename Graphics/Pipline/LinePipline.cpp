#include "LinePipline.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../Utils/Logger.h"
#include <cassert>

void LinePipline::Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler) {
	device_ = device;
	shaderCompiler_ = shaderCompiler;
	CreatePSO();
}

void LinePipline::CreatePSO() {
	CreateRootSignature();
	InputLayout();
	BlendState();
	RasterizerState();
	VertexShader();
	PixelShader();
	DepthStencilState();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.InputLayout = inputLayoutDesc_;
	desc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	desc.PS = { pixelShaderBlob_->GetBufferPointer(),  pixelShaderBlob_->GetBufferSize() };
	desc.BlendState = blendDesc_;
	desc.RasterizerState = rasterizerDesc_;

	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// ★ Triangleとの差分①: LINE
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	desc.DepthStencilState = depthStencilDesc_;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&graphicsPipelineState_));
	if (FAILED(hr)) {
		Logger::Log("LinePipline : Failed CreateGraphicsPipelineState\n");
	}
	assert(SUCCEEDED(hr));
}

void LinePipline::CreateRootSignature() {
	// Lineはテクスチャ不要なのでルートパラメータは2つのみ
	// [0] マテリアル（色） CBV  → PixelShader  b0
	// [1] WVP行列         CBV  → VertexShader b0
	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = rootParameters;
	desc.NumParameters = _countof(rootParameters);
	desc.pStaticSamplers = nullptr;  // サンプラー不要
	desc.NumStaticSamplers = 0;

	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		Logger::Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
	}
	assert(SUCCEEDED(hr));

	hr = device_->CreateRootSignature(0,
		signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	if (FAILED(hr)) {
		Logger::Log("LinePipline : Failed CreateRootSignature\n");
	}
	assert(SUCCEEDED(hr));
}

void LinePipline::InputLayout() {
	// ★ Triangleとの差分②: POSITIONのみ（TEXCOORDなし）
	inputElementDescs_[0].SemanticName = "POSITION";
	inputElementDescs_[0].SemanticIndex = 0;
	inputElementDescs_[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputLayoutDesc_.pInputElementDescs = inputElementDescs_;
	inputLayoutDesc_.NumElements = 1;
}

void LinePipline::BlendState() {
	blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

void LinePipline::RasterizerState() {
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
}

void LinePipline::VertexShader() {
	// ★ Triangleとの差分③: Line専用シェーダー
	vertexShaderBlob_.Attach(shaderCompiler_->CompileShader(L"HLSL/Line.VS.hlsl", L"vs_6_0"));
	assert(vertexShaderBlob_ != nullptr);
}

void LinePipline::PixelShader() {
	pixelShaderBlob_.Attach(shaderCompiler_->CompileShader(L"HLSL/Line.PS.hlsl", L"ps_6_0"));
	assert(pixelShaderBlob_ != nullptr);
}

void LinePipline::DepthStencilState() {
	depthStencilDesc_.DepthEnable = TRUE;
	depthStencilDesc_.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}