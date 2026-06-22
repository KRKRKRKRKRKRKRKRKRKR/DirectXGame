# DirectX12 用語集 43個 - 実装コード対応版

このドキュメントは、プロジェクト内で実際に使われている DirectX12 用語をコード上の場所と共に説明しています。

---

## グループ1：GPU メモリの基本（3個）

### 1. Heap（Default/Upload/Readback）

GPU が使うメモリ領域の種類。

| ヒープ種類 | 説明 | 用途 | コード上の場所 |
|-----------|------|------|-------------|
| **Default** | GPU だけが読み書きできる高速領域 | テクスチャ最終置き場 | `TextureManager.cpp:20, 85` |
| **Upload** | CPU が書き込んで GPU が読む | 頂点・定数バッファ転送用 | `TextureManager.cpp:45` |
| **Readback** | GPU が書き込んで CPU が読む | GPU 計算結果を CPU に返す | （このプロジェクトでは未使用） |

**実装例**

```cpp
// TextureManager.cpp:45 - Upload ヒープ
pResource = CreateBufferResource(device, sizeInBytes);
// ↑ 内部で D3D12_HEAP_TYPE_UPLOAD を使用
```

---

### 2. Resource（GPU メモリの塊）

GPU 上のメモリ領域そのものを表すオブジェクト（`ID3D12Resource`）。

**プロジェクト内の Resource 一覧**

| リソース名 | ファイル | 行番号 | サイズ | 内容 | 更新頻度 |
|-----------|---------|--------|--------|------|---------|
| `vertexResource_` | Triangle.h | 49 | 12頂点×24byte | 正四面体の位置・UV | 初期化時のみ |
| `materialResource_` | Triangle.h | 53 | 4096×16byte | RGBA 色データ | 毎フレーム |
| `wvpResource_` | Triangle.h | 59 | 4096×64byte | WVP 行列 | 毎フレーム |

**実装例**

```cpp
// Triangle.h:49-59
class Triangle : public IDrawable {
    ComPtr<ID3D12Resource> vertexResource_;      // 頂点バッファ
    ComPtr<ID3D12Resource> materialResource_;    // マテリアルバッファ
    ComPtr<ID3D12Resource> wvpResource_;         // WVP 行列バッファ
};
```

---

### 3. Buffer（汎用バイト列データ）

頂点データ・インデックスデータ・定数（行列・色）などを格納する Resource の一種。

**プロジェクト内の Buffer 一覧**

| Buffer 種類 | ファイル | 行番号 | 用途 |
|-----------|---------|--------|------|
| 頂点バッファ | Triangle.h | 49 | 正四面体の座標・UV |
| マテリアルバッファ | Triangle.h | 53 | RGBA 色 |
| WVP 行列バッファ | Triangle.h | 59 | ワールド・ビュー・プロジェクション |
| グリッド頂点バッファ | DirectXManager.cpp | 800+ | グリッド線の頂点 |

**実装例**

```cpp
// TextureManager.cpp:42-71 - CreateBufferResource
ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
    // UPLOAD ヒープ上にバッファ確保 → これが Buffer
    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = sizeInBytes,
        // ...
    };
}
```

---

## グループ2：View（リソースの「覗き窓」）（6個）

Resource をどのように使うかを定義する「覗き窓」のこと。同じ Resource でも View によって用途が変わる。

### 4. VertexBufferView（VBV）

頂点バッファ Resource をどう読み取るかを定義する View。「バッファのどこから始まるか・1頂点が何バイトか・全体が何バイトか」を GPU に伝える。

**実装例**

```cpp
// Triangle.h:50 - VBV 宣言
D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

// Triangle.cpp:195 - 実際に使用
commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
// ↑ ここで「このバッファから頂点を読む」と GPU に伝える
```

**VBV が持つ情報**

```cpp
D3D12_VERTEX_BUFFER_VIEW {
    BufferLocation   : vertexResource_ の GPU アドレス
    SizeInBytes      : 12頂点 × 24byte = 288byte
    StrideInBytes    : 1 頂点当たり 24 byte
}
```

---

### 5. ShaderResourceView（SRV）

テクスチャや構造化バッファをシェーダーから読み取るための View。PixelShader がテクスチャのデータにアクセスするために必要。

**実装例**

```cpp
// DescriptorHeaps.cpp:54 - SRV 作成
device_->CreateShaderResourceView(textureResource, &srvDesc, handle);

// DirectXManager.cpp:728 - SRV を GPU に設定
SetGraphicsRootDescriptorTable(2, GetTextureSrvHandle());
// ↑ ルートパラメータ[2]にテクスチャ SRV を渡す

// Object3d.PS.hlsl:3 - HLSL 側で受け取り
Texture2D<float4> gTexture : register(t0);

// Object3d.PS.hlsl:28 - 実際に使う
float4 color = gTexture.Sample(gSampler, input.texcoord);
```

**SRV の役割**

```
「このテクスチャを PixelShader が読める状態にしろ」
  ├─ フォーマット: RGBA8_UNORM など
  ├─ 最初の MipLevel: 0
  ├─ Mip 数: 1
  └─ Dimension: Texture2D
```

---

### 6. RenderTargetView（RTV）

描画結果の書き込み先（レンダーターゲット）を定義する View。通常はスワップチェーンのバックバッファを指し、ここに書き込んだ内容が画面に表示される。

**実装例**

```cpp
// DescriptorHeaps.cpp:25-27 - RTV 作成
CreateRenderTargetView(swapChainResources_[i], nullptr, rtvHandle);
// ↑ スワップチェーンの バッファ[0] と [1] に対して作成

// DirectXManager.cpp:164 - RTV をセット
OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
// ↑ 「これからこの RTV に描く」と指定
```

**RTV の役割**

```
毎フレーム：
  1. RTV[0] に描く（バックバッファ）
  2. Present() で GPU に「バッファを交換しろ」
  3. 次フレームは RTV[1] に描く（新しいバックバッファ）
  ← ダブルバッファリングでちらつき防止
```

| 処理 | ファイル | 行 | 内容 |
|-----|---------|-----|------|
| RTV 作成 | DescriptorHeaps.cpp | 25-27 | 2 個作成（スワップチェーン用） |
| RTV 設定 | DirectXManager.cpp | 164 | `OMSetRenderTargets()` |
| RTV 選択 | DirectXManager.cpp | 160 | `GetCurrentBackBufferIndex()` で 0 or 1 |

---

### 7. DepthStencilView（DSV）

Z-Buffer（深度バッファ）への書き込み先を定義する View。`OMSetRenderTargets()` に RTV と一緒に渡すことで、深度テストが有効になる。

**実装例**

```cpp
// DescriptorHeaps.cpp:34 - DSV 作成
CreateDepthStencilView(depthStencilResource, nullptr, dsvHandle);
// ↑ Z-バッファ用（1個だけ、すべてのフレームで共有）

// DirectXManager.cpp:163 - DSV を RTV と一緒にセット
OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

// DirectXManager.cpp:167 - 毎フレーム Z-バッファをクリア
ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0);
// ↑ 1.0f = 最も奥（初期値）
```

**Z-バッファの役割**

```
【正面の物体】Z = 0.5 ← 手前
  ↓
【背景】Z = 0.9 ← 奥

描画時に GPU が「0.5 < 0.9？ Yes → 描く」と判定
```

---

### 8. ConstantBufferView（CBV）

定数バッファ（行列・色など）をシェーダーから参照するための View。WVP 行列や色をシェーダーに渡すときに使う。

**実装例**

```cpp
// Triangle.cpp:203-205 - CBV（行列）をシェーダーに渡す
SetGraphicsRootConstantBufferView(0, materialGpuAddress);  // 色
SetGraphicsRootConstantBufferView(1, wvpGpuAddress);      // 行列

// Object3d.VS.hlsl:7 - HLSL 側で受け取り
cbuffer WVP : register(b0) {
    float4x4 viewProjection;
};

// Object3d.PS.hlsl:8 - PixelShader でも受け取り
cbuffer Material : register(b0) {
    float4 color;
};
```

**CBV のフロー**

```
Triangle::SetWvpMatrix(const Matrix4x4& matrix)
  ↓
wvpResource_ に行列データを CPU から書き込む
  ↓
DrawTriangleRender()
  ↓
SetGraphicsRootConstantBufferView(1, wvpAddress)
  ↓
GPU: "シェーダーが register(b1) で読んでくる"
  ↓
VertexShader が WVP 行列で座標変換
```

---

## グループ3：パイプライン設定（5個）

### 9. RootSignature（シェーダーへの入力契約）

シェーダーがどんなリソース（テクスチャ・定数バッファ）を受け取るかを定義する契約書のこと。PSO を作る前に先に決めておく必要がある。

**実装例**

```cpp
// Pipeline.cpp:76-112 - RootSignature 作成
D3D12_ROOT_PARAMETER rootParameters[3] = {
    // [0] = マテリアル色 (CBV)
    // [1] = WVP 行列 (CBV)
    // [2] = テクスチャ (DescriptorTable → SRV)
};

D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
    .NumParameters = 3,
    .pParameters = rootParameters,
    .NumStaticSamplers = 1,
    .pStaticSamplers = staticSamplers
};
```

**RootSignature が定義すること**

```
「シェーダーは以下を受け取る」
  ├─ register(b0) ← RootParameter[0] = マテリアル CBV
  ├─ register(b1) ← RootParameter[1] = WVP CBV
  ├─ register(t0) ← RootParameter[2] 内の SRV
  └─ register(s0) ← StaticSampler
```

---

### 10. RootParameter（スロット定義）

RootSignature の中の各スロット（番号）の定義のこと。「このスロットは定数バッファか、ディスクリプタテーブルか、定数か」を指定する。

**実装例**

```cpp
// Pipeline.cpp:80-92 - 各 RootParameter の定義
D3D12_ROOT_PARAMETER rootParameters[3];

// [0] = マテリアル色
rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
rootParameters[0].Descriptor.ShaderRegister = 0;
rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;  // PS のみ

// [1] = WVP 行列
rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
rootParameters[1].Descriptor.ShaderRegister = 1;
rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;  // VS のみ

// [2] = テクスチャ
rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange_[0];
```

**各パラメータの役割**

| # | 型 | 役割 | シェーダー側 |
|---|-----|------|---------|
| 0 | CBV | マテリアル色（PS） | `register(b0)` |
| 1 | CBV | WVP 行列（VS） | `register(b1)` |
| 2 | Table | テクスチャ（PS） | `register(t0)` |

---

### 11. PSO（PipelineStateObject）

レンダリングパイプライン全体の設定をまとめたオブジェクトのこと。使うシェーダー・ブレンド設定・ラスタライザ設定・深度設定・入力レイアウト・ルートシグネチャをすべて含む。

**実装例**

```cpp
// Pipeline.cpp:67 - PSO 作成
D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {
    .pRootSignature = rootSignature_.Get(),
    .VS = vertexShader,
    .PS = pixelShader,
    .BlendState = blendState,
    .RasterizerState = rasterizerState,
    .DepthStencilState = depthStencilState,
    .InputLayout = inputLayout,
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
};

// DirectXManager.cpp:751 - PSO を GPU にセット
SetPipelineState(pipeline_->GetPipelineState());
// ↑ これ 1 回で全設定がいっぺんに適用される
```

**PSO が持つもの**

```
PSO（1つのセット）
  ├─ RootSignature     : 入力構造
  ├─ VertexShader      : 座標変換プログラム
  ├─ PixelShader       : 色計算プログラム
  ├─ BlendState        : 色合成ルール
  ├─ RasterizerState   : ポリゴン→ピクセル ルール
  ├─ DepthStencilState : 深度テスト ルール
  └─ InputLayout       : 頂点フォーマット
```

---

### 12. InputLayout（頂点フォーマット）

頂点バッファのデータ構造をシェーダーに伝える設定のこと。「最初の 12 バイトが位置(XYZ)、次の 8 バイトが UV 座標」というような定義。

**実装例**

```cpp
// Pipeline.cpp:116-123 - 頂点フォーマット定義
D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {
    {
        "POSITION",
        0,
        DXGI_FORMAT_R32G32B32A32_FLOAT,  // Vector4（内部は 16byte）
        0,
        0,  // offset 0byte から
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0
    },
    {
        "TEXCOORD",
        0,
        DXGI_FORMAT_R32G32_FLOAT,  // Vector2（8byte）
        0,
        16,  // offset 16byte から（POSITION の後）
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        0
    }
};
```

**メモリレイアウト**

```
【1 頂点のデータ】全 24byte
┌─────────────────┬──────────┐
│ POSITION        │ TEXCOORD │
│ (16byte)        │ (8byte)  │
│ XYZW            │ UV       │
└─────────────────┴──────────┘
0byte             16byte     24byte

GPU が VBV を読むとき：
  - offset 0 から 16byte → POSITION
  - offset 16 から 8byte → TEXCOORD
```

---

## グループ4：細部設定（4個）

### 13. BlendState（色合成ルール）

ピクセルを描画するとき、すでに描かれている色とどう合成するかの設定のこと。半透明（アルファブレンド）や加算合成などをここで指定する。

**実装例**

```cpp
// Pipeline.cpp:129-131
D3D12_BLEND_DESC blendDesc = {
    .AlphaToCoverageEnable = FALSE,
    .IndependentBlendEnable = FALSE,
    .RenderTarget[0] = {
        .BlendEnable = FALSE,  // 半透明しない
        .SrcBlend = D3D12_BLEND_ONE,
        .DestBlend = D3D12_BLEND_ZERO,
        .BlendOp = D3D12_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D12_BLEND_ONE,
        .DestBlendAlpha = D3D12_BLEND_ZERO,
        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL  // 全色成分書き込み
    }
};
```

**このプロジェクトの設定**

```
BlendEnable = FALSE
  ↓
新しい色が既存色を 100% 上書きする（半透明しない）
```

---

### 14. RasterizerState（ポリゴン→ピクセル変換）

3D のポリゴンを 2D のピクセルに変換する処理（ラスタライズ）の設定のこと。カリング（裏面を描かない）・塗りつぶしモード（ワイヤーフレームか塗りつぶしか）・深度バイアスなどを指定する。

**実装例**

```cpp
// Pipeline.cpp:134-137
D3D12_RASTERIZER_DESC rasterizerDesc = {
    .FillMode = D3D12_FILL_MODE_SOLID,      // 塗りつぶし（WIREFRAME なら枠だけ）
    .CullMode = D3D12_CULL_MODE_NONE,       // 両面描画（背面も描く）
    .DepthBias = 0,
    .DepthBiasClamp = 0.0f,
    .SlopeScaledDepthBias = 0.0f,
    .DepthClipEnable = TRUE,
    .MultisampleEnable = FALSE,
    .AntialiasedLineEnable = FALSE
};
```

**設定項目**

| 項目 | 値 | 意味 |
|-----|-----|------|
| FillMode | SOLID | 塗りつぶし |
| CullMode | NONE | 両面描画（背面も描く） |

---

### 15. DepthStencilState（深度テスト設定）

深度テスト（Z-Buffer）とステンシルテストの設定のこと。「深度テストを有効にするか」「深度値を書き込むか」「どの比較関数を使うか」などを指定する。

**実装例**

```cpp
// Pipeline.cpp:152-156
D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
    .DepthEnable = TRUE,
    .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
    .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
    .StencilEnable = FALSE,
    .FrontFace = { ... },
    .BackFace = { ... }
};
```

---

### 16. DepthFunc（深度比較関数）

Z-Buffer の深度比較をどの方法で行うかの関数のこと。通常は `LESS`（新しいピクセルの深度が小さい＝手前にある場合だけ書き込む）を使う。

**実装例**

```cpp
// Pipeline.cpp:155 に埋め込み
DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL

// その他のオプション
D3D12_COMPARISON_FUNC_LESS       // 新 <  既存（標準）
D3D12_COMPARISON_FUNC_EQUAL      // 新 == 既存
D3D12_COMPARISON_FUNC_GREATER    // 新 >  既存（奥側優先）
D3D12_COMPARISON_FUNC_ALWAYS     // 常に描く（深度無視）
```

---

## グループ5：命令送信（3個）

### 17. CommandList（GPU 命令の記録）

GPU への描画命令をためておくリストのこと（`ID3D12GraphicsCommandList`）。CPU 側で「頂点をセットせよ・パイプラインをセットせよ・描画せよ」という命令を順番に記録しておき、まとめて GPU に送る。

**実装例**

```cpp
// DirectXManager.h:130 - CommandList 宣言
ComPtr<ID3D12GraphicsCommandList> commandList_;

// DirectXManager.cpp:146-177 - BeginFrame で初期化
commandList_->Reset(commandAllocator_.Get(), nullptr);  // リセット
commandList_->OMSetRenderTargets(1, &rtvHandle, ...);   // RTV 指定
commandList_->ClearRenderTargetView(...);                // クリア

// DirectXManager.cpp:600+ - 描画命令を積む
commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);
commandList_->DrawInstanced(...);  // ← 命令として記録されるだけ

// DirectXManager.cpp:179 - EndFrame で終了
commandList_->Close();  // 記録終了
```

**CommandList のライフサイクル**

```
【毎フレーム】

1. Reset()          → 前フレームの命令をクリア、新たに記録開始
2. [描画命令を積む] → IASetVertexBuffers, SetPipeline, DrawInstanced...
3. Close()          → 記録終了
4. ExecuteCommandLists() → GPU に送信
5. 次フレーム → Reset() でリセット
```

---

### 18. CommandQueue（GPU への投入）

CommandList を GPU に送って実行させるキューのこと（`ID3D12CommandQueue`）。`ExecuteCommandLists()` で記録済みの CommandList を GPU に投入する。

**実装例**

```cpp
// DirectXManager.h:127 - CommandQueue 宣言
ComPtr<ID3D12CommandQueue> commandQueue_;

// DirectXManager.cpp:186 - GPU に実行させる
ID3D12CommandList* commandLists[] = { commandList_.Get() };
commandQueue_->ExecuteCommandLists(1, commandLists);
```

**フロー**

```
DirectXManager::EndFrame()
  ↓
commandList_->Close()                    // 記録終了
  ↓
commandQueue_->ExecuteCommandLists()     // GPU 実行
  ↓
"GPU: CommandList の命令を実行開始"
```

---

### 19. SwapChain（ダブルバッファリング）

画面に表示するバッファを管理する仕組みのこと（`IDXGISwapChain`）。描画中のバッファと表示中のバッファを交互に切り替えて、ちらつきなく画面を更新する。

**実装例**

```cpp
// DirectXManager.cpp:338 - SwapChain 作成
DXGI_SWAP_CHAIN_DESC swapChainDesc = {
    .BufferCount = 2,                    // 2 つのバッファ
    .BufferDesc.Width = width,
    .BufferDesc.Height = height,
    .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD
};

// DirectXManager.cpp:160 - 現在のバッファ番号を取得
UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
// ↑ 0 か 1 が返る

// DirectXManager.cpp:187 - Vsync で交換
swapChain_->Present(1, 0);
// ↑ 引数 1 = Vsync ON（60FPS に同期）
```

**ダブルバッファリングの流れ**

```
【フレーム N】
  CPU/GPU:   buffer[0] に描く
  画面:      buffer[1] を表示

【フレーム N+1】
  Present(1, 0) ← buffer を交換
  CPU/GPU:   buffer[1] に描く
  画面:      buffer[0] を表示

メリット：描いている最中に、描き終わった方を画面に表示
  → ちらつきなし
```

---

## グループ6：シェーダー・テクスチャ（8個）

### 20. VertexShader（頂点座標変換プログラム）

各頂点の座標変換を行う GPU 上のプログラム（シェーダー）のこと。ワールド・ビュー・プロジェクション行列（WVP）を掛けて、3D 座標をスクリーン上の 2D 座標に変換する。

**実装例**

```cpp
// Pipeline.cpp:141 - コンパイル
CompileShader(device, "Shaders/Object3d.VS.hlsl", "vs_6_0");
```

```hlsl
// HLSL/Object3d.hlsli - 入出力構造体
struct VS_Input {
    float4 position : POSITION;   // 頂点位置
    float2 texcoord : TEXCOORD0;  // テクスチャ座標
};

struct VS_Output {
    float4 position : SV_POSITION;  // 画面上の 2D 座標（GPU が計算）
    float2 texcoord : TEXCOORD0;    // テクスチャ座標（PixelShader へ）
};

// HLSL/Object3d.VS.hlsl
cbuffer WVP : register(b1) {
    float4x4 viewProjection;
};

VS_Output main(VS_Input input) {
    VS_Output output;
    // 【重要】3D 座標 → 2D スクリーン座標に変換
    output.position = mul(input.position, viewProjection);
    output.texcoord = input.texcoord;
    return output;
}
```

**VertexShader の役割**

```
【入力】3D座標（-100, 50, 200）
  ↓
【計算】WVP 行列を掛ける
  output.position = input.position × WVP行列
  ↓
【出力】スクリーン座標（450, 300）← 画面上のピクセル位置

この計算は GPU が各頂点ごとに並列実行
```

---

### 21. PixelShader（ピクセルの色を決定）

ラスタライズ後の各ピクセルの色を決定する GPU 上のプログラム（シェーダー）のこと。テクスチャのサンプリング・ライティング・色の計算などを行う。

**実装例**

```cpp
// Pipeline.cpp:147 - コンパイル
CompileShader(device, "Shaders/Object3d.PS.hlsl", "ps_6_0");
```

```hlsl
// HLSL/Object3d.PS.hlsl
cbuffer Material : register(b0) {
    float4 color;  // マテリアル色
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VS_Output input) : SV_TARGET {
    // テクスチャの色を読み込む
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // マテリアル色と掛け合わせ
    float4 finalColor = textureColor * color;
    
    return finalColor;
}
```

**PixelShader の役割**

```
【入力】各ピクセルの テクスチャ座標（UV）
  ↓
【計算】UV から テクスチャ色を読む → マテリアル色と合成
  ↓
【出力】最終的なピクセルの色（RGBA）

この計算も GPU が各ピクセルごとに並列実行
```

---

### 22. HLSL（GPU シェーダー言語）

DirectX のシェーダーを書くための言語（High-Level Shader Language）のこと。C 言語に近い構文で GPU 上の処理を記述できる。

**実装例**

```
Shaders/ 配下のファイル一覧

Object3d.hlsli       : 共有ヘッダー（構造体定義）
Object3d.VS.hlsl     : VertexShader プログラム
Object3d.PS.hlsl     : PixelShader プログラム

LinePipeline.VS.hlsl : Line 用VertexShader
LinePipeline.PS.hlsl : Line 用PixelShader
```

**HLSL の言語特性**

```hlsl
// C 言語に近い構文
float4 position : POSITION;  // 入力

// シェーダー固有
mul(vector, matrix)          // 行列積
float4 : SV_TARGET           // 画面出力
register(b0), register(t0)   // GPU リソース指定
```

---

### 23. Dxc（DirectX Shader Compiler）

HLSL ファイルを GPU が実行できるバイトコード（DXIL）にコンパイルするツール・ライブラリのこと。旧来の `fxc` より新しく、DirectX12 では基本的に `Dxc` を使う。

**実装例**

```cpp
// ShaderCompiler.cpp - DXC でコンパイル
ComPtr<IDxcCompiler3> dxcCompiler_;  // DXC コンパイラ
ComPtr<IDxcLibrary> dxcLibrary_;

// コンパイル処理
dxcCompiler_->Compile(
    sourceBlob,        // HLSL テキスト
    arguments,
    "vs_6_0",         // ← ターゲット：VertexShader 6.0
    &result
);
```

**DXC の仕事**

```
入力：HLSL テキストファイル（Object3d.VS.hlsl）
  ↓
処理：HLSL → DXIL（GPU が実行できるバイトコード）
  ↓
出力：GPU が実行できるシェーダーバイナリ
```

---

### 24. register（HLSL リソース指定）

HLSL でシェーダーが受け取るリソースをどのスロットに割り当てるかを指定するキーワードのこと。`b0`（定数バッファ 0 番）、`t0`（テクスチャ 0 番）、`s0`（サンプラー 0 番）のように指定する。

**実装例**

```hlsl
// Object3d.VS.hlsl
cbuffer WVP : register(b1) {    // ← register(b1) = 定数バッファ 1 番
    float4x4 viewProjection;
};

// Object3d.PS.hlsl
cbuffer Material : register(b0) {   // ← register(b0) = 定数バッファ 0 番
    float4 color;
};

Texture2D<float4> gTexture : register(t0);  // ← register(t0) = テクスチャ 0 番
SamplerState gSampler : register(s0);      // ← register(s0) = サンプラー 0 番
```

**register と RootParameter の対応**

| HLSL | C++ |
|------|-----|
| `register(b0)` | RootParameter[0] CBV |
| `register(b1)` | RootParameter[1] CBV |
| `register(t0)` | RootParameter[2] SRV |
| `register(s0)` | StaticSampler |

---

### 25. Sampler（テクスチャ読み取り補間）

テクスチャを読み取るときの補間方法を定義するオブジェクトのこと。拡大縮小時にどう補間するか（線形補間・ニアレスト）、UV が範囲外のときどうするか（繰り返し・クランプ）などを指定する。

**実装例**

```cpp
// Pipeline.cpp:78-92 - Sampler 定義
D3D12_STATIC_SAMPLER_DESC samplerDesc = {
    .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // 線形補間
    .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // U 軸：端で止まる
    .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // V 軸：端で止まる
    .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    .MipLODBias = 0.0f,
    .MaxAnisotropy = 1,
    .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
    .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
    .MinLOD = 0.0f,
    .MaxLOD = D3D12_FLOAT32_MAX,
    .ShaderRegister = 0,
    .RegisterSpace = 0,
    .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL  // PS だけが使う
};

// HLSL で使用
float4 color = gTexture.Sample(gSampler, input.texcoord);
//                              ↑ ここで Sampler が補間方法を指定
```

**Sampler の設定項目**

| 設定 | 値 | 意味 |
|------|-----|------|
| Filter | LINEAR | テクスチャ読み時に周辺ピクセルを補間 |
| AddressU/V | CLAMP | UV が 1.0 を超えたら端で止まる |
| AddressU/V | WRAP | UV が 1.0 を超えたら繰り返し |

---

### 26. TextureCoordinate（UV）

テクスチャのどの位置を頂点に対応させるかを示す 2D 座標のこと。U が横方向（0.0〜1.0）、V が縦方向（0.0〜1.0）で、テクスチャ全体を 0〜1 の範囲で表す。

**実装例**

```cpp
// Triangle.cpp - 頂点データに UV を含む
struct Vertex {
    Vector4 position;   // 3D 位置
    Vector2 texcoord;   // ← UV 座標
};

// 正四面体 12 頂点分
Vertex vertices[12] = {
    { {0, 1, 0, 1},    {0.5f, 0.0f} },  // 頂点 1: UV = (0.5, 0.0)
    { {1, 0, 1, 1},    {1.0f, 1.0f} },  // 頂点 2: UV = (1.0, 1.0)
    { {0, -1, 0, 1},   {0.5f, 1.0f} },  // 頂点 3: UV = (0.5, 1.0)
    // ...
};
```

**UV 座標の意味**

```
UV 空間（テクスチャ）
┌──────────────────┐
│ (0,0)      (1,0)│
│  ┌───────────┐   │
│  │テクスチャ │   │
│  │  画像    │   │
│  └───────────┘   │
│ (0,1)      (1,1)│
└──────────────────┘

（0.5, 0.5） = テクスチャの中央
（0.0, 0.0） = テクスチャの左上
（1.0, 1.0） = テクスチャの右下
```

---

### 27. Sampling（テクスチャ読み取り）

シェーダーがテクスチャの特定の UV 座標の色を読み取る処理のこと。Sampler の設定に従って、UV 位置の周辺の Texel を補間して色を返す。

**実装例**

```hlsl
// Object3d.PS.hlsl
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VS_Output input) : SV_TARGET {
    // Sample = テクスチャ読み取り
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    //                                     ↑ UV 座標
    
    return textureColor * color;
}
```

**Sampling の内部処理**

```
入力：
  gTexture = テクスチャ画像
  gSampler = 補間ルール（LINEAR など）
  input.texcoord = UV 座標（例：0.35, 0.72）

処理：
  1. Sampler が「補間方法は LINEAR」を確認
  2. UV (0.35, 0.72) の周辺 4 ピクセルを読み込む
  3. 線形補間で合成色を計算
  
出力：
  補間された色（float4）
```

---

## グループ7：その他（12個）

### 28. Texture（画像リソース）

画像データを格納する Resource のこと。2D の配列として幅・高さ・フォーマットを持つ。ポリゴンの表面に貼り付けて見た目を表現するために使う。

**実装例**

```cpp
// TextureManager.h:27 - テクスチャ管理
std::unordered_map<TextureID, ComPtr<ID3D12Resource>> textureResources_;

// TextureManager.cpp:85 - テクスチャ読み込み・GPU へ転送
ID3D12Resource* LoadTexture(const std::string& filepath) {
    // 1. PNG/JPG を読み込む（DirectXTex）
    // 2. GPU メモリ (DEFAULT_HEAP) に転送
    // 3. SRV を作成
}

// DirectXManager.cpp:728 - テクスチャ設定
SetGraphicsRootDescriptorTable(2, descriptorHeaps_.GetTextureSrvHandle(textureID_));
```

**テクスチャの流れ**

```
ファイル（PNG）
  ↓ DirectXTex で読み込み
CPU メモリ（画像ピクセル列）
  ↓ GPU に転送
GPU メモリ（DEFAULT_HEAP のテクスチャ）
  ↓ SRV 経由で PixelShader が Sample()
PixelShader で色計算
```

---

### 29. Z-Buffer（DepthBuffer）

各ピクセルの「奥行き値（深度）」を格納する特殊なバッファのこと。手前にあるオブジェクトが奥のオブジェクトを正しく隠す（前後関係の解決）ために使う。描画のたびに深度値を比較して、より手前のピクセルだけを残す。

**実装例**

```cpp
// DirectXManager.cpp:167 - 毎フレーム Z-バッファをクリア
ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0);
// ↑ 1.0f = 最も奥（初期値）
```

**Z-バッファの役割**

```
【正面の物体】Z = 0.5 ← 手前
  ↓
【背景】Z = 0.9 ← 奥

描画時に GPU が「0.5 < 0.9？ Yes → 描く」と判定
```

---

### 30. DoubleBuffering（ダブルバッファリング）

画面に表示しているバッファと、GPU が描き込んでいるバッファを 2 枚に分ける仕組みのこと。GPU が書き込み中のバッファが画面に映らないようにして、ちらつきを防ぐ。SwapChain がこれを実現している。

**実装例**

```cpp
// DirectXManager.cpp:160 - 現在のバッファを取得
UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
// ↑ 0 か 1 が返る

// DirectXManager.cpp:187 - Vsync で交換
swapChain_->Present(1, 0);
// ↑ 引数 1 = Vsync ON（60FPS に同期）
```

**流れ**

```
【フレーム N】
  CPU/GPU:   buffer[0] に描く
  画面:      buffer[1] を表示

【フレーム N+1】
  Present(1, 0) ← buffer を交換
  CPU/GPU:   buffer[1] に描く
  画面:      buffer[0] を表示

メリット：描いている最中に、描き終わった方を画面に表示
  → ちらつきなし
```

---

### 31. Viewport（描画領域指定）

レンダリング結果をウィンドウのどの矩形領域に表示するかの設定のこと。左上の座標・幅・高さ・最小/最大深度を指定する。複数のビューポートを使うと画面分割表示なども可能。

**実装例**

```cpp
// DirectXManager.cpp:169-170 - Viewport 設定
D3D12_VIEWPORT viewport = {
    .TopLeftX = 0.0f,
    .TopLeftY = 0.0f,
    .Width = (float)windowWidth_,
    .Height = (float)windowHeight_,
    .MinDepth = 0.0f,
    .MaxDepth = 1.0f
};

commandList_->RSSetViewports(1, &viewport);
```

**Viewport の役割**

```
【スクリーン座標】GPU が出力する 2D 座標
  (-1, -1) ────→ (1, -1)
     ↓              ↓
  (-1,  1) ────→ (1,  1)

【Viewport】スクリーン座標をウィンドウのどこに表示するか
  (0, 0) ─────────→ (width, 0)
    ↓                    ↓
  (0, height) ───→ (width, height)
```

---

### 32. Scissor（描画範囲切り取り）

描画するピクセルを矩形範囲で切り取る設定のこと。Viewport と似ているが、Scissor は Viewport の中でさらに描画を限定するために使う。Viewport と Scissor は常にセットで設定する必要がある。

**実装例**

```cpp
// DirectXManager.cpp:173 - Scissor 設定
D3D12_RECT scissorRect = {
    .left = 0,
    .top = 0,
    .right = windowWidth_,
    .bottom = windowHeight_
};

commandList_->RSSetScissorRects(1, &scissorRect);
```

**Scissor の役割**

```
Viewport の中でさらに描画を限定
  例：ウィンドウ右半分だけ描く
      Scissor = { left: width/2, right: width }

game_screen.Scissor = { 左上から 512×512 }
  → 左上 512×512 の範囲だけ描画、他は スキップ
```

---

### 33. Transform（位置・回転・スケール）

オブジェクトの位置・回転・スケールをまとめた変換情報のこと。この 3 つからワールド行列（`MakeAffineMatrix`）を作り、頂点シェーダーに渡す。

**実装例**

```cpp
// Math/MathTypes.h
struct Transform {
    Vector3 scale;       // スケール（倍率）
    Vector3 rotation;    // 回転（ラジアン）
    Vector3 translation; // 平行移動（位置）
};

// Game/Game.cpp:30-35 - Transform の初期化
triangleTransforms_[i].scale = Vector3(1.0f, 1.0f, 1.0f);
triangleTransforms_[i].rotation = Vector3(0.0f, 0.0f, 0.0f);
triangleTransforms_[i].translation = Vector3(0.0f, 70.0f - (i * 3.0f), 0.0f);

// Game/Game.cpp:39-40 - Transform の更新
transform1_.rotation.y += 60.0f * deltaTime;  // Y 軸で回転

// DirectXManager.cpp:600+ - Transform から行列を作成
Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
```

**Transform から行列への変換**

```
Transform
  ├─ scale: (1, 1, 1)
  ├─ rotation: (0, 0, 30°)
  └─ translation: (10, 20, 30)
       ↓ MakeAffineMatrix()
Matrix4x4（4x4 行列）
  ├─ Scale 成分
  ├─ Rotation 成分
  └─ Translation 成分
       ↓ WVP 行列と掛ける
スクリーン座標
```

---

### 34. Material（見た目パラメータ）

オブジェクトの色・テクスチャなど見た目に関するパラメータのまとまりのこと。DirectX の仕様ではなく、エンジン設計上の概念。

**実装例**

```cpp
// DirectXManager.cpp:640+ - Material を設定
void DrawTriangleRender(
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projectionMatrix,
    const Transform& transform,
    const Vector4& materialColor,  // ← Material（色）
    TextureID textureID
) {
    // Material を GPU に書き込み
    triangle_->SetColor(materialColor);  // Color Buffer に書く
}

// Object3d.PS.hlsl
cbuffer Material : register(b0) {
    float4 color;  // 色データ
};

float4 main(...) : SV_TARGET {
    float4 textureColor = gTexture.Sample(gSampler, ...);
    return textureColor * color;  // ← Material と合成
}
```

**Material がまとめるもの**

```
Material（オブジェクトの見た目）
  ├─ color: Vector4（RGBA）
  ├─ texture: TextureID
  ├─ （将来）metallic: float
  ├─ （将来）roughness: float
  └─ （将来）normal map: TextureID
```

---

### 35. Scene（ゲーム画面単位）

ゲーム内の 1 画面分のオブジェクト・カメラ・ライトなどをまとめた単位のこと。「タイトル画面」「ゲーム画面」「リザルト画面」などがそれぞれ 1 シーンになる。

**実装例**

```cpp
// Game/Game.h - Scene の実装
class Game {
    // 1 シーンのオブジェクト
    Transform transform1_;
    Transform transform2_;
    TrailParticle3D trailParticles_[20];
    
    void Initialize(...);  // シーン初期化
    void Update(float deltaTime);  // シーン更新
    void Render();  // シーン描画
};

// Engine/Engine.cpp - 複数シーンの切り替え（将来）
game_.Update(deltaTime);
game_.Render();
// → 将来は Scene[] に増やす
```

**Scene の役割**

```
【ゲーム設計上】
  1 シーン = 1 画面分のゲーム状態
  
例：Race the Sun
  シーン 0 = タイトル画面
  シーン 1 = ゲーム画面
  シーン 2 = リザルト画面
  
各シーンは独立した
  - オブジェクト
  - カメラ
  - ライト
  - ロジック
```

---

### 36. Log（実行状況出力）

プログラムの動作状況を記録・出力する仕組みのこと。バグの調査・実行状況の確認に使う。DirectX のエラーコードも Log に出力することで原因を特定しやすくなる。

**実装例**

```cpp
// Engine/Utils/Logger.h
class Logger {
public:
    static void Log(const std::string& message);
};

// Logger.cpp
void Logger::Log(const std::string& message) {
    std::cout << message << std::endl;
    OutputDebugStringA(message.c_str());  // VS デバッガーにも出力
}

// 使用例：DirectXManager.cpp:100
Logger::Log("DirectXManager initialized successfully");
Logger::Log("Triangle initialized with " + std::to_string(kMaxInstanceCount) + " instances");
```

**Log の用途**

```
【初期化時】
  Logger::Log("GPU: NVIDIA RTX 3090");
  Logger::Log("SwapChain: 1280x720, Format: SRGB");

【デバッグ時】
  Logger::Log("DrawCall count: " + std::to_string(drawCallCount));
  Logger::Log("GPU memory used: " + std::to_string(gpuMemory) + " MB");

【エラー検出】
  if (failed) Logger::Log("ERROR: Shader compilation failed");
```

---

### 37. ImGui（デバッグ UI ライブラリ）

ゲームやツールの開発中に使う即時描画型の GUI ライブラリのこと。コードだけでスライダー・テキスト・ボタンを作れる。リリース版では外す。

**実装例**

```cpp
// Game/Game.cpp:81-121 - ImGui 使用
void Game::DrawImGui() {
    ImGui::Begin("FPS");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
    
    ImGui::Begin("Trail Settings");
    ImGui::SliderFloat("Fall Speed", &trailParam_.fallSpeed, 0.01f, 2000.0f);
    ImGui::SliderFloat("Goal Area Radius", &trailParam_.goalAreaRadius, 0.0f, 300.0f);
    ImGui::Combo("Texture", reinterpret_cast<int*>(&textureID_), "None\0Texture1\0");
    ImGui::End();
}

// Externals/imgui/imguiManager.cpp - 描画
imgui_.BeginFrame();
// ... ゲーム描画 ...
imgui_.EndFrame(&directX_);  // ImGui 命令を GPU に記録
```

**ImGui の特徴**

```
【即時描画型 GUI】
  - コード 1 行で UI 追加
  - 毎フレーム描画（状態保持不要）
  
【用途】
  - パラメータ調整スライダー
  - FPS 表示
  - デバッグ情報出力
  
【本番リリース時】
  - 削除する（ImGui は開発用ツール）
```

---

### 38. PIX（デバッグ・プロファイリングツール）

Microsoft が提供する DirectX のデバッグ・プロファイリングツールのこと。1 フレームの描画命令をキャプチャして、どの DrawCall が重いか・GPU の使用状況を可視化できる。パフォーマンス改善時に使う。

**実装例**

```cpp
// 今のプロジェクトでは使っていない
// が、プロトコル的には対応可能

// 使い方：
// 1. Visual Studio で PIX 起動
// 2. exe を選択
// 3. 実行ボタン → 1 フレームキャプチャ
// 4. DrawCall 単位で GPU 時間を確認
```

**PIX でできること**

```
【DrawCall 分析】
  - 各 DrawCall が何秒かかったか
  - GPU の 3D パイプラインがボトルネック？
  - メモリ帯域幅がボトルネック？

【メモリ分析】
  - GPU メモリ使用量
  - キャッシュ効率

【命令分析】
  - CommandList の内容
  - リソースバインディング
```

---

### 39-43. その他補足（Texel, Descriptor など）

| 用語 | 説明 |
|------|------|
| **Texel** | テクスチャを構成する 1 つ 1 つの画素のこと。画像の「ピクセル」に対してテクスチャ版の呼び名。Texture + Pixel = Texel。 |
| **DescriptorTable** | 複数のディスクリプタ（View の情報）をまとめて 1 つの RootParameter として扱う仕組みのこと。 |
| **DescriptorRange** | DescriptorTable の中の「何番から何個分のディスクリプタを使うか」を定義するもの。 |
| **DescriptorHeaps** | すべての View（RTV・DSV・SRV など）をまとめて管理する領域のこと。 |

---

## 🎯 全43個の最終チェックリスト

### グループ1：GPU メモリ（3個）
- ✅ Heap → TextureManager.cpp:45
- ✅ Resource → Triangle.h:49, 53, 59
- ✅ Buffer → TextureManager.cpp:42-71

### グループ2：View（6個）
- ✅ VertexBufferView → Triangle.cpp:195
- ✅ ShaderResourceView → DescriptorHeaps.cpp:54
- ✅ RenderTargetView → DescriptorHeaps.cpp:25-27
- ✅ DepthStencilView → DescriptorHeaps.cpp:34
- ✅ ConstantBufferView → Triangle.cpp:203-205
- ✅ View (概念) → グループ2 先頭参照

### グループ3：パイプライン設定（5個）
- ✅ RootSignature → Pipeline.cpp:76-112
- ✅ RootParameter → Pipeline.cpp:80-92
- ✅ PSO → Pipeline.cpp:67
- ✅ InputLayout → Pipeline.cpp:116-123
- ✅ PipelineStateObject → PSO 参照

### グループ4：細部設定（4個）
- ✅ BlendState → Pipeline.cpp:129-131
- ✅ RasterizerState → Pipeline.cpp:134-137
- ✅ DepthStencilState → Pipeline.cpp:152-156
- ✅ DepthFunc → Pipeline.cpp:155

### グループ5：命令送信（3個）
- ✅ CommandList → DirectXManager.cpp:146-187
- ✅ CommandQueue → DirectXManager.cpp:186
- ✅ SwapChain → DirectXManager.cpp:187

### グループ6：シェーダー・テクスチャ（8個）
- ✅ VertexShader → Pipeline.cpp:141
- ✅ PixelShader → Pipeline.cpp:147
- ✅ HLSL → Shaders/ 配下
- ✅ Dxc → ShaderCompiler.cpp
- ✅ register → Object3d.hlsl
- ✅ Sampler → Pipeline.cpp:78-92
- ✅ TextureCoordinate → Triangle.cpp
- ✅ Sampling → Object3d.PS.hlsl:28

### グループ7：その他（12個）
- ✅ Texture → TextureManager.h/cpp
- ✅ Z-Buffer → DirectXManager.cpp:167
- ✅ DoubleBuffering → DirectXManager.cpp:160, 187
- ✅ Viewport → DirectXManager.cpp:170
- ✅ Scissor → DirectXManager.cpp:173
- ✅ Transform → Math/MathTypes.h
- ✅ Material → DirectXManager.cpp:640+
- ✅ Scene → Game/Game.h
- ✅ Log → Engine/Utils/Logger.h
- ✅ ImGui → Game/Game.cpp:81-121
- ✅ PIX → （デバッグツール）
- ✅ Texel / Descriptor 等 → グループ7 末尾参照

---

## 📊 全体フロー図

```
【CPU側】
  CommandList に命令を記録
    ├─ RootSignature + PSO をセット
    │   └─ PSO の中に InputLayout / BlendState / RasterizerState / DepthStencilState
    │
    ├─ Viewport / Scissor をセット
    │
    ├─ VBV / IBV（頂点・インデックス）をセット
    │
    ├─ CBV / SRV をセット
    │   └─ RootParameter / DescriptorTable / register で繋ぐ
    │
    ├─ DrawCall 実行

【GPU側】
  VertexShader
    ├─ Transform（WVP行列）で 3D→2D に変換
    ├─ 頂点出力
    
  ラスタライズ（RasterizerState で制御）
    └─ ポリゴン→ピクセル分解
    
  PixelShader
    ├─ Sampler で Texture から UV 座標の色を Sampling
    ├─ Material（色パラメータ）と合成
    └─ 最終色を出力
    
  OutputMerger
    ├─ DepthFunc / DepthStencilState で Z-Buffer テスト
    ├─ BlendState で色合成
    └─ RTV（バックバッファ）に書き込み

【画面】
  SwapChain でダブルバッファリング
  Viewport で矩形指定
  → 画面に表示
```

---

**作成日：2026-06-22**  
**対象プロジェクト：DirectXGame（Race the Sun クローン）**  
**バージョン：1.0**
