#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include "BlendMode.h"
using Microsoft::WRL::ComPtr;

class ShaderCompiler;

// Pipeline/SkinnedPipeline共通の骨組み（Template Methodパターン）。
// BlendState/RasterizerState/DepthStencilState/CreatePSOのループは全PSO種別で同一のため
// ここに実装し、ディスクリプタレンジ数・ルートシグネチャ・InputLayout・VertexShaderという
// 「実際のシェーダー・頂点フォーマットに依存する部分」だけを派生クラスに委ねる。
class PipelineBase {
public:
	PipelineBase() = default;
	virtual ~PipelineBase() = default;

	void Initialize(ID3D12Device* device, ShaderCompiler* shaderCompiler, bool enableDepth = true);

	ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

	// enableAlphaTest=falseのPSOはPS内にdiscard命令自体が存在しないバリアントを使う。
	// これによりαTestを使わない大多数の描画（グリッド等）でGPUドライバのEarly-Z最適化が有効なままになる
	ID3D12PipelineState* GetPipelineState(BlendMode blendMode = BlendMode::kNone, bool enableAlphaTest = false) const {
		return graphicsPipelineStates_[static_cast<size_t>(blendMode)][enableAlphaTest ? 1 : 0].Get();
	}

protected:
	static constexpr UINT STATIC_SAMPLER_COUNT = 1;

	// ---- 派生クラスが実装する差分部分 ----

	// t0テクスチャ/t1 WVP/t2 色に加え、派生クラス固有のSRVレンジ(t3ボーン行列パレット等)を
	// descriptorRange_に書き込む。返り値は書き込んだレンジ数（NumDescriptorRangesの計算等では使わない、
	// デバッグ用の目安）
	virtual void CreateDescriptorRange() = 0;

	// ルートシグネチャを作成し rootSignature_/signatureBlob_ に格納する
	virtual void CreateRootSignature() = 0;

	// inputLayoutDesc_ を組み立てる
	virtual void InputLayout() = 0;

	// vertexShaderBlob_ にコンパイル結果を格納する
	virtual void VertexShader() = 0;

	// ---- 全PSO種別で共通の処理（オーバーライド任意）----

	virtual void CreateStaticSamplers();
	// 全種別ともObject3d.PS.hlslを使うためデフォルト実装を用意。enableAlphaTest=trueなら
	// ENABLE_ALPHA_TESTマクロを付けてコンパイルし、PS内にdiscard命令があるバリアントを作る
	virtual void PixelShader(bool enableAlphaTest);
	virtual void BlendState(BlendMode blendMode);
	virtual void RasterizerState();
	virtual void DepthStencilState(BlendMode blendMode);

	ComPtr<ID3D12Device> device_;
	ShaderCompiler* shaderCompiler_ = nullptr;
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[STATIC_SAMPLER_COUNT] = {};

	bool enableDepth_ = true;
	ComPtr<ID3DBlob> signatureBlob_ = nullptr;
	ComPtr<ID3DBlob> errorBlob_ = nullptr;
	ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	// [BlendMode][0=αTestなし/1=αTestあり]
	ComPtr<ID3D12PipelineState> graphicsPipelineStates_[static_cast<size_t>(BlendMode::kCount)][2];
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc_ = {};
	D3D12_BLEND_DESC blendDesc_ = {};
	D3D12_RASTERIZER_DESC rasterizerDesc_ = {};
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc_ = {};

	ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
	ComPtr<IDxcBlob> pixelShaderBlob_ = nullptr; // 直近のPixelShader(enableAlphaTest)呼び出し結果を一時的に保持

private:
	// PSOをBlendMode::kCount個作る共通ループ。派生固有部分(CreateRootSignature/InputLayout/
	// VertexShader/PixelShader)を呼んだ後、BlendState/DepthStencilStateをBlendModeごとに切り替えながら
	// CreateGraphicsPipelineStateを繰り返す
	void CreatePSO();
};
