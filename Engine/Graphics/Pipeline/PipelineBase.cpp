#include "PipelineBase.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../../Utils/Logger.h"
#include <cassert>

void PipelineBase::Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler, bool enableDepth) {
	device_ = device;
	shaderCompiler_ = shaderCompiler;
	enableDepth_ = enableDepth;
	CreateDescriptorRange();
	CreateStaticSamplers();
	CreatePSO();
}

void PipelineBase::CreateStaticSamplers() {
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers_[0].ShaderRegister = 0;
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
}

void PipelineBase::PixelShader(bool enableAlphaTest) {
	std::vector<std::wstring> defines;
	if (enableAlphaTest) {
		defines.push_back(L"ENABLE_ALPHA_TEST");
	}
	pixelShaderBlob_.Attach(shaderCompiler_->CompileShader(L"HLSL/Object3D.PS.hlsl", L"ps_6_0", defines));
	assert(pixelShaderBlob_ != nullptr);
}

// BlendStateの設定（blendModeごとにSrc/Dest/Opの組み合わせを切り替える）
void PipelineBase::BlendState(BlendMode blendMode) {
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
		// 通常のアルファブレンド： Src*a + Dest*(1-a)
		// a は PSが出力するper-pixelのアルファ値（テクスチャのカバレッジ×gBlendStrength、
		// PS側でgBlendStrengthを乗算済み）。文字の輪郭など画素ごとの透明度がそのまま活きる
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kAdd:
		// 加算： Src*a + Dest*1
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_ONE;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		break;

	case BlendMode::kSubtract:
		// 減算： Dest*1 - Src*a
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
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

void PipelineBase::RasterizerState() {
	rasterizerDesc_ = {};
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc_.DepthClipEnable = TRUE;
}

// kNone以外（半透明合成）は深度テストのみ・深度書き込みなし（DepthWriteMask=ZERO）にする。
// 深度を書き込むと、色は薄く合成されても奥のオブジェクトが深度テストで棄却されて描かれなくなるため
void PipelineBase::DepthStencilState(BlendMode blendMode) {
	depthStencilDesc_ = {};
	depthStencilDesc_.DepthEnable = enableDepth_ ? TRUE : FALSE;
	bool writeDepth = enableDepth_ && (blendMode == BlendMode::kNone);
	depthStencilDesc_.DepthWriteMask = writeDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

// BlendState・DepthStencilStateが異なるPSOをブレンドモード×αTest有無の組み合わせ分作る。
// αTest無効のバリアントはPS内にdiscard命令自体が存在しないため、GPUドライバのEarly-Z最適化が
// 効いたまま描画できる（discardが静的に存在するだけでEarly-Zを無効化するドライバが多いため、
// 実行時にαTestを使わないオブジェクトでも別バリアントに分けることで恩恵を受けられる）
void PipelineBase::CreatePSO() {
	CreateRootSignature();
	InputLayout();
	RasterizerState();
	VertexShader();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc_;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	for (int alphaTestIndex = 0; alphaTestIndex < 2; ++alphaTestIndex) {
		bool enableAlphaTest = (alphaTestIndex == 1);
		PixelShader(enableAlphaTest);
		graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };

		for (size_t i = 0; i < static_cast<size_t>(BlendMode::kCount); ++i) {
			BlendMode blendMode = static_cast<BlendMode>(i);
			BlendState(blendMode);
			graphicsPipelineStateDesc.BlendState = blendDesc_;

			DepthStencilState(blendMode);
			graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc_;

			HRESULT hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStates_[i][alphaTestIndex]));
			if (FAILED(hr)) {
				Logger::Log("Failed CreateGraphicsPipelineState (BlendMode/AlphaTest)\n");
			}
			assert(SUCCEEDED(hr));
		}
	}
}
