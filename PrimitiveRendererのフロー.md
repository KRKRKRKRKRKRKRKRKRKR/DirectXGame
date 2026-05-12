PrimitiveRenderer::Initialize()の生成手順

//================================
//DXC関連
//================================

[InitializeDXC]
DirectX Shader Compiler（DXC）を初期化する
dxcUtils_を生成
dxcCompiler_を生成
includeHandler_を生成

//================================
PSO関連
//================================

[CreatePSO]
Pipeline State Objectを生成する。
PSOは、シェーダーやレンダーステートなどのグラフィックスパイプラインの設定をまとめたオブジェクトです。
いろんな設定を行う
pixelShader_を生成
vertexShader_を生成
rootSignature_を生成


[CreateRootSignature]
Root Signatureを生成する。
Root Signatureは、シェーダーがアクセスするリソース（定数バッファ、シェーダーリソースビュー、サンプラーなど）を定義するオブジェクトです。
errorBlob_を生成
signatureBlob_を生成

[InputLayout]
Input Layoutを生成する。
Input Layoutは、頂点データの構造を定義するオブジェクトです。

[BlendState]
Blend Stateを生成する。
Blend Stateは、ピクセルシェーダーの出力とフレームバッファの既存の値をどのように組み合わせるかを定義するオブジェクトです。

[ResterizerState]
Rasterizer Stateを生成する。
Rasterizer Stateは、ジオメトリをどのようにラスタライズするかを定義するオブジェクトです。

[VertexShader]
Vertex Shaderを生成する。
Vertex Shaderは、頂点データを処理するシェーダーです。

[PixelShader]
Pixel Shaderを生成する。
Pixel Shaderは、ピクセルデータを処理するシェーダーです。

//================================
//Resource関連
//================================

[CreateVertexResource]

頂点バッファを生成する。
頂点バッファは、頂点データを格納するためのリソースです。

[CreateVertexBufferView]
頂点バッファビューを生成する。
頂点バッファビューは、頂点バッファの一部を指定してアクセスするためのオブジェクトです。
