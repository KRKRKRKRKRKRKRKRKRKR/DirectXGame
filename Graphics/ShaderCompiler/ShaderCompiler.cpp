#include "ShaderCompiler.h"
#include "../../Utils/Logger.h"
#include "../../Utils/StringUtils.h"
#include <cassert>
#include <format>
#pragma region DXCの初期化
//DXCの初期化
void ShaderCompiler::InitializeDXC() {
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
#pragma endregion

#pragma region シェーダーのコンパイル
//シェーダーをコンパイル
IDxcBlob* ShaderCompiler::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
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
#pragma endregion

#pragma region CompileShaderのサブ関数
//シェーダーファイルの読み込み
void ShaderCompiler::LoadHLSLFile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource) {
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

//シェーダーのコンパイル実行
void ShaderCompiler::ExecuteCompile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource, IDxcResult*& shaderResult) {
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
		includeHandler_.Get(),
		IID_PPV_ARGS(&shaderResult)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed Compile\n");
	}
	assert(SUCCEEDED(hr));
}

//コンパイルエラーのログ出力
void ShaderCompiler::LogCompileErrors(IDxcResult* shaderResult) {
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Logger::Log(StringUtils::ConvertString(std::format("Shader Compile Error: {}\n", shaderError->GetStringPointer())));
		assert(false);
	}
	if (shaderError) {
		shaderError->Release();
	}
}

//コンパイル結果からシェーダーブロブの取得
IDxcBlob* ShaderCompiler::GetShaderBlob(const std::wstring& filePath, const wchar_t* profile, IDxcResult* shaderResult) {
	IDxcBlob* shaderBlob = nullptr;
	HRESULT hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

	if (FAILED(hr)) {
		Logger::Log("Failed GetOutput for shader blob\n");
	}
	assert(SUCCEEDED(hr));

	Logger::Log(StringUtils::ConvertString(std::format(L"Complete CompileShader, path: {}, profile: {}\n", filePath, profile)));
	return shaderBlob;

}
#pragma endregion
