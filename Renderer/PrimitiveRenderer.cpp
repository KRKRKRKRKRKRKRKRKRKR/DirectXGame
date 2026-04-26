#include "PrimitiveRenderer.h"
#include "../Utils/Logger.h"
#include "../Utils/StringUtils.h"
#include <cassert>
#include <format>

//==================================================================
//デストラクタ
//==================================================================
PrimitiveRenderer::~PrimitiveRenderer() {
	Finalize();
}

//==================================================================
// 初期化
//==================================================================
void PrimitiveRenderer::Initialize(DirectXManager* dx) {
	InitializeDXC();
	CreatePSO(dx);
	CreateVertexResource(dx);
	CreateMaterialResource(dx);
	Logger::Log("Complete Initialize PrimitiveRenderer\n");
}

//==================================================================
// 終了処理
//==================================================================
void PrimitiveRenderer::Finalize() {
	if (materialResource_) { materialResource_->Release();      materialResource_ = nullptr; }
	if (vertexResource_) { vertexResource_->Release();       vertexResource_ = nullptr; }
	if (pixelShaderBlob_) { pixelShaderBlob_->Release();       pixelShaderBlob_ = nullptr; }
	if (vertexShaderBlob_) { vertexShaderBlob_->Release();      vertexShaderBlob_ = nullptr; }
	if (graphicsPipelineState_) { graphicsPipelineState_->Release(); graphicsPipelineState_ = nullptr; }
	if (rootSignature_) { rootSignature_->Release();         rootSignature_ = nullptr; }
	if (signatureBlob_) { signatureBlob_->Release();         signatureBlob_ = nullptr; }
	if (errorBlob_) { errorBlob_->Release();             errorBlob_ = nullptr; }
	if (includeHandler_) { includeHandler_->Release();        includeHandler_ = nullptr; }
	if (dxcCompiler_) { dxcCompiler_->Release();           dxcCompiler_ = nullptr; }
	if (dxcUtils_) { dxcUtils_->Release();              dxcUtils_ = nullptr; }
}

//==================================================================
//DXC関連
//==================================================================
void PrimitiveRenderer::InitializeDXC() {

	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	if (FAILED(hr)) {
		Logger::Log("Failed DxcCreateInstance for IDxcUtils\n");
	}
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	if (FAILED(hr)) {
		Logger::Log("Failed DxcCreateInstance for IDxcCompiler3\n");
	}
	assert(SUCCEEDED(hr));

	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDefaultIncludeHandler\n");
	}
	assert(SUCCEEDED(hr));
}

IDxcBlob* PrimitiveRenderer::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
	IDxcBlobEncoding* shaderSource = nullptr;
	IDxcResult* shaderResult = nullptr;
	LoadHLSLFile(filePath, profile, shaderSource);
	ExecuteCompile(filePath, profile, shaderSource, shaderResult);
	LogCompileErrors(shaderResult);

	IDxcBlob* blob = GetShaderBlob(filePath, profile, shaderResult);

	if (shaderResult) { shaderResult->Release(); }
	if (shaderSource) { shaderSource->Release(); }

	return blob;
}

void PrimitiveRenderer::LoadHLSLFile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource) {
	Logger::Log(StringUtils::ConvertString(std::format(L"Begin CompileShader, path: {}, profile: {}\n", filePath, profile)));
	HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);

	if (FAILED(hr)) {
		Logger::Log("Failed LoadFile\n");
	}

	assert(SUCCEEDED(hr));

	shaderSourceBuffer_.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer_.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer_.Encoding = DXC_CP_UTF8;
}

void PrimitiveRenderer::ExecuteCompile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource, IDxcResult*& shaderResult) {
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi",L"-Qembed_debug",
		L"-Od",
		L"-Zpr",
	};

	HRESULT hr = dxcCompiler_->Compile(
		&shaderSourceBuffer_,
		arguments,
		_countof(arguments),
		includeHandler_,
		IID_PPV_ARGS(&shaderResult)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed Compile\n");
	}

	assert(SUCCEEDED(hr));

}

void PrimitiveRenderer::LogCompileErrors(IDxcResult* shaderResult) {
	IDxcBlobUtf8* shaderError = nullptr;

	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Logger::Log(StringUtils::ConvertString(std::format("Shader Compile Error: {}\n", shaderError->GetStringPointer())));
		shaderError->Release();
		assert(false);
	}
	if (shaderError) {
		shaderError->Release();
	}
}

IDxcBlob* PrimitiveRenderer::GetShaderBlob(const std::wstring& filePath, const wchar_t* profile, IDxcResult* shaderResult) {
	IDxcBlob* shaderBlob = nullptr;
	HRESULT hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

	if (FAILED(hr)) {
		Logger::Log("Failed GetOutput for shader blob\n");
	}
	assert(SUCCEEDED(hr));

	Logger::Log(StringUtils::ConvertString(std::format(L"Complete CompileShader, path: {}, profile: {}\n", filePath, profile)));
	return shaderBlob;
}

//==================================================================
// PSO関連
//==================================================================
void PrimitiveRenderer::CreatePSO(DirectXManager* dx) {
	CreateRootSignature(dx);
	InputLayout();
	BlendState();
	RasterizerState();
	VertexShader();
	PixelShader();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_;
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

	HRESULT hr = dx->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateGraphicsPipelineState\n");
	}
	assert(SUCCEEDED(hr));

}

void PrimitiveRenderer::CreateRootSignature(DirectXManager* dx) {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		Logger::Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
	}
	assert(SUCCEEDED(hr));

	hr = dx->GetDevice()->CreateRootSignature(0,
		signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));

	if (FAILED(hr)) {
		Logger::Log("Failed CreateRootSignature\n");
	}
	assert(SUCCEEDED(hr));
}

void PrimitiveRenderer::InputLayout() {
	inputElementDescs_[0].SemanticName = "POSITION";
	inputElementDescs_[0].SemanticIndex = 0;
	inputElementDescs_[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputLayoutDesc_.pInputElementDescs = inputElementDescs_;
	inputLayoutDesc_.NumElements = _countof(inputElementDescs_);
}

void PrimitiveRenderer::BlendState() {
	blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

void PrimitiveRenderer::RasterizerState() {
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
}

void PrimitiveRenderer::VertexShader() {
	vertexShaderBlob_ = CompileShader(L"Object3D.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob_ != nullptr);
}

void PrimitiveRenderer::PixelShader() {
	pixelShaderBlob_ = CompileShader(L"Object3D.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob_ != nullptr);
}

//==================================================================
//リソース関連
//==================================================================
ID3D12Resource* PrimitiveRenderer::CreateBufferResource(DirectXManager* dx, size_t sizeInBytes) {
	ID3D12Resource* resource = nullptr;
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC vertexBufferResourceDesc{};
	vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexBufferResourceDesc.Width = sizeInBytes;
	vertexBufferResourceDesc.Height = 1;
	vertexBufferResourceDesc.DepthOrArraySize = 1;
	vertexBufferResourceDesc.MipLevels = 1;
	vertexBufferResourceDesc.SampleDesc.Count = 1;
	vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = dx->GetDevice()->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));
	return resource;
}

void PrimitiveRenderer::CreateVertexResource(DirectXManager* dx) {
	vertexResource_ = CreateBufferResource(dx, sizeof(Vector4) * 3);
	CreateVertexBufferView();
	WriteVertexResource();
}

void PrimitiveRenderer::CreateVertexBufferView() {
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(Vector4) * 3;
	vertexBufferView_.StrideInBytes = sizeof(Vector4);
}

void PrimitiveRenderer::WriteVertexResource() {
	Vector4* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	vertexData[0] = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[1] = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
	vertexData[2] = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
	vertexResource_->Unmap(0, nullptr);
}

void PrimitiveRenderer::CreateMaterialResource(DirectXManager* dx) {
	materialResource_ = CreateBufferResource(dx, sizeof(Vector4));
	Vector4* materialData = nullptr;
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	*materialData = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
	materialResource_->Unmap(0, nullptr);
}

//==================================================================
//描画関連
//==================================================================
void PrimitiveRenderer::DrawTriangleRender(DirectXManager* dx, int32_t width, int32_t height, ID3D12GraphicsCommandList* commandList) {
	ViewportScissorRect(width, height);
	commandList->RSSetViewports(1, &viewport_);
	commandList->RSSetScissorRects(1, &scissorRect_);
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(graphicsPipelineState_);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->DrawInstanced(3, 1, 0, 0);
}

void PrimitiveRenderer::ViewportScissorRect(int32_t width, int32_t height) {
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
	scissorRect_.left = 0;
	scissorRect_.right = width;
	scissorRect_.top = 0;
	scissorRect_.bottom = height;
}

