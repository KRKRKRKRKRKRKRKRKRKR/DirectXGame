#include "Pipeline.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../Utils/Logger.h"
#include <cassert>

//Piplineの初期化
void Pipeline::Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler, bool enableDepth) {
	device_ = device;
    shaderCompiler_ = shaderCompiler;
	enableDepth_ = enableDepth;
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

	// BlendState・DepthStencilStateが異なるPSOをブレンドモードの数だけ作る
	// ルートシグネチャ・シェーダー・ラスタライザ等は全モード共通なのでここでは使い回す
	for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCount); ++i) {
		BlendMode blendMode = static_cast<BlendMode>(i);
		BlendState(blendMode);
		graphicsPipelineStateDesc.BlendState = blendDesc_;

		// kNone以外（半透明合成）は深度テストのみ行い、深度書き込みはしない。
		// 書き込むと、色は透けて見えても奥のオブジェクトが深度テストで棄却されてしまうため
		DepthStencilState(blendMode);
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc_;

		HRESULT hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates_[i]));
		if (FAILED(hr)) {
			Logger::Log("Failed CreateGraphicsPipelineState (BlendMode)\n");
		}
		assert(SUCCEEDED(hr));
	}
}

#pragma region PSOの内部関数
//PSOのルートシグネチャの作成
void Pipeline::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[6] = {};

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

	// [4] b1: インスタンスオフセット（VS）- SV_InstanceIDはStartInstanceLocationでオフセットされないため必要
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[4].Constants.ShaderRegister = 1;
	rootParameters[4].Constants.RegisterSpace = 0;
	rootParameters[4].Constants.Num32BitValues = 1;

	// [5] b2: ブレンド/αTest情報（PS）- kMultiply/kScreenはBlendFactor定数では強さ調整できないため、
	// PS側でSrcColorを補間する。x=blendMode(int), y=blendStrength, z=enableAlphaTest(bool), w=alphaThreshold
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Constants.ShaderRegister = 2;
	rootParameters[5].Constants.RegisterSpace = 0;
	rootParameters[5].Constants.Num32BitValues = 4;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	descriptionRootSignature.pStaticSamplers = staticSamplers_;
	descriptionRootSignature.NumStaticSamplers = STATIC_SAMPLER_COUNT;

	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		if (errorBlob_) {
			Logger::Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		} else {
			Logger::Log("Pipeline: D3D12SerializeRootSignature failed (no error blob)\n");
		}
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

//PSOのBlendStateの設定（blendModeごとにSrc/Dest/Opの組み合わせを切り替える）
void Pipeline::BlendState(BlendMode blendMode) {
	blendDesc_ = {};
	D3D12_RENDER_TARGET_BLEND_DESC& rt = blendDesc_.RenderTarget[0];
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// アルファ(SrcBlendAlpha/DestBlendAlpha)は全モード共通で「Srcのアルファをそのまま書く」にしておく
	// （このエンジンではRTVのアルファ値を後段で使っていないため、カラーの合成式だけ考えればよい）
	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.DestBlendAlpha = D3D12_BLEND_ZERO;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	switch (blendMode) {
	case BlendMode::kNone:
	default:
		// ブレンドしない＝Srcでそのまま上書き（従来の挙動）
		rt.BlendEnable = FALSE;
		break;

	case BlendMode::kNormal:
		// 通常のアルファブレンド： Src*s + Dest*(1-s)
		// s は per-pixelのアルファ値ではなく、描画直前に commandList->OMSetBlendFactor() で
		// 指定する「定数」。D3D12_BLEND_(INV_)BLEND_FACTOR がその定数を参照する
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_BLEND_FACTOR;
		rt.DestBlend = D3D12_BLEND_INV_BLEND_FACTOR;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kAdd:
		// 加算： Src*s + Dest*1
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_BLEND_FACTOR;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kSubtract:
		// 減算： Dest*1 - Src*s
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_BLEND_FACTOR;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		break;

	case BlendMode::kMultiply:
		// 乗算： Dest*SrcColor（Srcの寄与はゼロにして掛け算だけ残す）
		// NOTE: DestBlendはSrcColorという「ピクセルごとに違う値」を参照するため、
		// OMSetBlendFactorの定数１つでは s=0(効果なし)〜s=1(全開)を補間できない
		// （固定機能ブレンダーの制約）。そのためこのモードは強さ調整の対象外
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_ZERO;
		rt.DestBlend = D3D12_BLEND_SRC_COLOR;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kScreen:
		// スクリーン： Src*1 + Dest*(1-SrcColor)
		// kMultiplyと同じ理由でDestBlendがSrcColor依存のため強さ調整の対象外
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_ONE;
		rt.DestBlend = D3D12_BLEND_INV_SRC_COLOR;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;
	}
}

//PSOのRasterizerStateの設定
void Pipeline::RasterizerState() {
	rasterizerDesc_ = {};
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc_.DepthClipEnable = TRUE;
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
// kNone以外（半透明合成）は深度テストのみ・深度書き込みなし（DepthWriteMask=ZERO）にする。
// 深度を書き込むと、色は薄く合成されても奥のオブジェクトが深度テストで棄却されて描かれなくなるため
void Pipeline::DepthStencilState(BlendMode blendMode) {
	depthStencilDesc_ = {};
	depthStencilDesc_.DepthEnable = enableDepth_ ? TRUE : FALSE;
	bool writeDepth = enableDepth_ && (blendMode == BlendMode::kNone);
	depthStencilDesc_.DepthWriteMask = writeDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}
#pragma endregion
#pragma	endregion