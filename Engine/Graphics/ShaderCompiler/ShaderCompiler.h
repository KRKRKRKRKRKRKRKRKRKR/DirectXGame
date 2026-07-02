#pragma once
#include <Windows.h>
#include <dxcapi.h>
#include <string>
#include <vector>
#include <wrl.h>

using Microsoft::WRL::ComPtr;
class ShaderCompiler {
public:
	ShaderCompiler() = default;
	~ShaderCompiler() = default;

	// DXCの初期化
	void InitializeDXC();

	// シェーダーをコンパイル。definesは"NAME"または"NAME=VALUE"形式のマクロ定義（-Dとして渡す）。
	// 同じHLSLファイルから条件分岐でPSバリアントを作る（例: ENABLE_ALPHA_TEST）用途
	IDxcBlob* CompileShader(const std::wstring& filePath, const wchar_t* profile, const std::vector<std::wstring>& defines = {});

private:

	//シェーダーファイルの読み込み
	void LoadHLSLFile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource);
	//シェーダーのコンパイル実行
	void ExecuteCompile(const std::wstring& filePath, const wchar_t* profile, const std::vector<std::wstring>& defines, IDxcBlobEncoding*& shaderSource, IDxcResult*& shaderResult);
	//コンパイルエラーのログ出力
	void LogCompileErrors(IDxcResult* shaderResult);
	//コンパイル結果からシェーダーブロブの取得
	IDxcBlob* GetShaderBlob(const std::wstring& filePath, const wchar_t* profile, IDxcResult* shaderResult);

	ComPtr<IDxcUtils> dxcUtils_ = nullptr;
	ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;
	ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
	DxcBuffer shaderSourceBuffer_;
};