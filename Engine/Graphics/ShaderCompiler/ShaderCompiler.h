#pragma once
#include <Windows.h>
#include <dxcapi.h>
#include <string>
#include <wrl.h>

using Microsoft::WRL::ComPtr;
class ShaderCompiler {
public:
	ShaderCompiler() = default;
	~ShaderCompiler() = default;
	
	// DXCの初期化
	void InitializeDXC();

	//シェーダーをコンパイル
	IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile);

private:

	//シェーダーファイルの読み込み
	void LoadHLSLFile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource);
	//シェーダーのコンパイル実行
	void ExecuteCompile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource, IDxcResult*& shaderResult);
	//コンパイルエラーのログ出力
	void LogCompileErrors(IDxcResult* shaderResult);
	//コンパイル結果からシェーダーブロブの取得
	IDxcBlob* GetShaderBlob(const std::wstring& filePath, const wchar_t* profile, IDxcResult* shaderResult);

	ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	DxcBuffer shaderSourceBuffer_;
};