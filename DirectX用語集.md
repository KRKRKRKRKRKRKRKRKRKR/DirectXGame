# DirectX 用語集（全43個）

---

## Heap（Default/Upload/Readback）
GPU が使うメモリ領域の種類のこと。
- **Default**：GPU だけが読み書きできる高速領域。テクスチャや頂点バッファの最終置き場
- **Upload**：CPU が書き込んで GPU が読む領域。CPU→GPU へデータを転送するときに使う
- **Readback**：GPU が書き込んで CPU が読む領域。GPU の計算結果を CPU に返すときに使う

自分のコードでは `CreateBufferResource()` で Upload ヒープを使って頂点バッファや WVP 行列を CPU から書き込んでいる。

---

## Resource
GPU 上のメモリ領域そのものを表すオブジェクト（`ID3D12Resource`）のこと。
テクスチャ・頂点バッファ・定数バッファなど、GPU が扱うデータはすべて Resource として管理される。
「何のデータか」ではなく「GPU メモリ上の塊」として抽象化されている。

自分のコードでは `vertexResource_`、`wvpResource_`、`materialResource_` などがこれにあたる。

---

## Buffer
汎用的なデータを格納する Resource のこと。テクスチャのように次元を持たない、バイト列のデータ。
頂点データ・インデックスデータ・定数（行列・色）などを格納する。

自分のコードでは `vertexResource_`（頂点バッファ）や `wvpResource_`（行列バッファ）が Buffer にあたる。

---

## Texture
画像データを格納する Resource のこと。2D の配列として幅・高さ・フォーマットを持つ。
ポリゴンの表面に貼り付けて見た目を表現するために使う。

自分のコードでは `TextureManager` がテクスチャの読み込みと GPU への転送を管理している。

---

## Z-Buffer（DepthBuffer）
各ピクセルの「奥行き値（深度）」を格納する特殊なバッファのこと。
手前にあるオブジェクトが奥のオブジェクトを正しく隠す（前後関係の解決）ために使う。
描画のたびに深度値を比較して、より手前のピクセルだけを残す。

自分のコードでは `ClearDepthStencilView()` で毎フレーム Z-Buffer をクリアしている。

---

## View
Resource をどのように使うかを定義する「覗き窓」のこと。
同じ Resource でも、テクスチャとして使うか、レンダーターゲットとして使うかで View が異なる。
DirectX12 では Resource 自体は汎用で、View によって用途が決まる。

---

## VertexBufferView（VBV）
頂点バッファ Resource をどう読み取るかを定義する View のこと。
「バッファのどこから始まるか・1頂点が何バイトか・全体が何バイトか」を GPU に伝える。

自分のコードでは `vertexBufferView_` がこれで、`IASetVertexBuffers()` でセットしている。

---

## ShaderResourceView（SRV）
テクスチャや構造化バッファをシェーダーから読み取るための View のこと。
ピクセルシェーダーがテクスチャのデータにアクセスするために必要。

自分のコードでは `TextureManager` が SRV を作成し、`SetGraphicsRootDescriptorTable()` でシェーダーに渡している。

---

## RenderTargetView（RTV）
描画結果の書き込み先（レンダーターゲット）を定義する View のこと。
通常はスワップチェーンのバックバッファを指し、ここに書き込んだ内容が画面に表示される。

自分のコードでは `descriptorHeaps_.CreateRTV()` で作成し、`OMSetRenderTargets()` でセットしている。

---

## DepthStencilView（DSV）
Z-Buffer（深度バッファ）への書き込み先を定義する View のこと。
`OMSetRenderTargets()` に RTV と一緒に渡すことで、深度テストが有効になる。

自分のコードでは `descriptorHeaps_.GetDSVHandle()` で取得して毎フレームセットしている。

---

## ConstantBufferView（CBV）
定数バッファ（行列・色など）をシェーダーから参照するための View のこと。
WVP 行列や色をシェーダーに渡すときに使う。
DirectX12 では `SetGraphicsRootConstantBufferView()` で直接 GPU アドレスを渡す方法も多い。

自分のコードでは `SetGraphicsRootConstantBufferView(1, wvpAddress)` で WVP 行列を渡している。

---

## RootSignature
シェーダーがどんなリソース（テクスチャ・定数バッファ）を受け取るかを定義する契約書のこと。
「このシェーダーはルートパラメータ0番に色、1番に行列、2番にテクスチャを受け取る」というような取り決め。
PSO を作る前に先に決めておく必要がある。

自分のコードでは `Pipeline::Initialize()` の中で `CreateRootSignature()` を呼んで作成している。

---

## RootParameter
RootSignature の中の各スロット（番号）の定義のこと。
「このスロットは定数バッファか、ディスクリプタテーブルか、定数か」を指定する。

自分のコードでは RootParameter[0]=色(CBV)、[1]=WVP行列(CBV)、[2]=テクスチャ(DescriptorTable) という構成になっている。

---

## DescriptorTable
複数のディスクリプタ（View の情報）をまとめて1つの RootParameter として扱う仕組みのこと。
テクスチャを複数まとめてシェーダーに渡すときなどに使う。

自分のコードではテクスチャの SRV を DescriptorTable 経由でシェーダーに渡している。

---

## DescriptorRange
DescriptorTable の中の「何番から何個分のディスクリプタを使うか」を定義するもの。
SRV・CBV・UAV のどの種類かも指定する。

自分のコードでは `descriptorRange_[0]` に SRV 1個分の範囲を定義している。

---

## CommandList
GPU への描画命令をためておくリストのこと（`ID3D12GraphicsCommandList`）。
CPU 側で「頂点をセットせよ・パイプラインをセットせよ・描画せよ」という命令を順番に記録しておき、まとめて GPU に送る。

自分のコードでは `commandList_->` で始まる一連の命令がこれ。`BeginFrame()` から `EndFrame()` まで命令を記録している。

---

## CommandQueue
CommandList を GPU に送って実行させるキューのこと（`ID3D12CommandQueue`）。
`ExecuteCommandLists()` で記録済みの CommandList を GPU に投入する。

自分のコードでは `EndFrame()` の `commandQueue_->ExecuteCommandLists()` がこれ。

---

## SwapChain
画面に表示するバッファを管理する仕組みのこと（`IDXGISwapChain`）。
描画中のバッファと表示中のバッファを交互に切り替えて、ちらつきなく画面を更新する。

自分のコードでは `swapChain_->Present(1, 0)` でフレーム終了時にバッファを交換している。

---

## DoubleBuffering
画面に表示しているバッファと、GPU が描き込んでいるバッファを2枚に分ける仕組みのこと。
GPU が書き込み中のバッファが画面に映らないようにして、ちらつきを防ぐ。
SwapChain がこれを実現している。

自分のコードでは `swapChainResources_[0]` と `[1]` の2枚が交互に使われる。

---

## RenderingPipeline
頂点データが画面のピクセルになるまでの一連の処理の流れのこと。
「頂点入力 → 頂点シェーダー → ラスタライズ → ピクセルシェーダー → 出力」という順序で処理が進む。

自分のコードでは PSO にこのパイプライン全体の設定がまとまっている。

---

## PipelineStateObject（PSO）
レンダリングパイプライン全体の設定をまとめたオブジェクトのこと。
使うシェーダー・ブレンド設定・ラスタライザ設定・深度設定・入力レイアウト・ルートシグネチャをすべて含む。
1回の描画でどんな見た目にするかの設定書。

自分のコードでは `Pipeline::Initialize()` で PSO を作成し、`SetPipelineState()` で適用している。

---

## BlendState
ピクセルを描画するとき、すでに描かれている色とどう合成するかの設定のこと。
半透明（アルファブレンド）や加算合成などをここで指定する。

自分のコードでは PSO の `BlendState` に半透明設定が入っている。

---

## RasterizerState
3D のポリゴンを2D のピクセルに変換する処理（ラスタライズ）の設定のこと。
カリング（裏面を描かない）・塗りつぶしモード（ワイヤーフレームか塗りつぶしか）・深度バイアスなどを指定する。

自分のコードでは PSO の `RasterizerState` でカリングと塗りつぶし設定を行っている。

---

## DepthStencilState
深度テスト（Z-Buffer）とステンシルテストの設定のこと。
「深度テストを有効にするか」「深度値を書き込むか」「どの比較関数を使うか」などを指定する。

---

## DepthFunc
Z-Buffer の深度比較をどの方法で行うかの関数のこと。
通常は `LESS`（新しいピクセルの深度が小さい＝手前にある場合だけ書き込む）を使う。

---

## InputLayout
頂点バッファのデータ構造をシェーダーに伝える設定のこと。
「最初の12バイトが位置(XYZ)、次の8バイトがUV座標」というような定義。

自分のコードでは PSO の `InputLayout` に頂点の `position` と `texcoord` の定義が入っている。

---

## VertexShader
各頂点の座標変換を行う GPU 上のプログラム（シェーダー）のこと。
ワールド・ビュー・プロジェクション行列（WVP）を掛けて、3D 座標をスクリーン上の2D 座標に変換する。

自分のコードでは `Object3d.VS.hlsl` がこれで、`wvpMatrix` を受け取って `output.position` に変換している。

---

## PixelShader
ラスタライズ後の各ピクセルの色を決定する GPU 上のプログラム（シェーダー）のこと。
テクスチャのサンプリング・ライティング・色の計算などを行う。

自分のコードでは `Object3d.PS.hlsl` がこれで、テクスチャ色とマテリアル色を掛け合わせてピクセルの色を決めている。

---

## HLSL
DirectX のシェーダーを書くための言語（High-Level Shader Language）のこと。
C 言語に近い構文で GPU 上の処理を記述できる。コンパイルすると GPU が実行できるバイトコードになる。

自分のコードでは `.hlsl` ファイルがこれ。`Dxc` でコンパイルしている。

---

## Dxc（DirectX Shader Compiler）
HLSL ファイルを GPU が実行できるバイトコード（DXIL）にコンパイルするツール・ライブラリのこと。
旧来の `fxc` より新しく、DirectX12 では基本的に `Dxc` を使う。

自分のコードでは `ShaderCompiler` クラスが `Dxc` を使って `.hlsl` をコンパイルしている。

---

## register
HLSL でシェーダーが受け取るリソースをどのスロットに割り当てるかを指定するキーワードのこと。
`b0`（定数バッファ0番）、`t0`（テクスチャ0番）、`s0`（サンプラー0番）のように指定する。
RootSignature のスロット番号と対応している必要がある。

---

## Sampler
テクスチャを読み取るときの補間方法を定義するオブジェクトのこと。
拡大縮小時にどう補間するか（線形補間・ニアレスト）、UV が範囲外のときどうするか（繰り返し・クランプ）などを指定する。

自分のコードでは RootSignature の `staticSamplers_` に線形補間・クランプ設定のサンプラーを定義している。

---

## TextureCoordinate（UV）
テクスチャのどの位置を頂点に対応させるかを示す2D 座標のこと。
U が横方向（0.0〜1.0）、V が縦方向（0.0〜1.0）で、テクスチャ全体を 0〜1 の範囲で表す。

自分のコードでは頂点データの `texcoord` メンバがこれで、`Vector2` で格納している。

---

## Texel
テクスチャを構成する1つ1つの画素のこと。画像の「ピクセル」に対してテクスチャ版の呼び名。
Texture + Pixel = Texel。

---

## Sampling
シェーダーがテクスチャの特定の UV 座標の色を読み取る処理のこと。
Sampler の設定に従って、UV 位置の周辺の Texel を補間して色を返す。

自分のコードでは PixelShader の `gTexture.Sample(gSampler, input.texcoord)` がこれ。

---

## Viewport
レンダリング結果をウィンドウのどの矩形領域に表示するかの設定のこと。
左上の座標・幅・高さ・最小/最大深度を指定する。
複数のビューポートを使うと画面分割表示なども可能。

自分のコードでは Task 3 で `BeginFrame()` に移動した `RSSetViewports(1, &viewport_)` がこれ。

---

## Scissor
描画するピクセルを矩形範囲で切り取る設定のこと。
Viewport と似ているが、Scissor は Viewport の中でさらに描画を限定するために使う。
Viewport と Scissor は常にセットで設定する必要がある。

自分のコードでは `RSSetScissorRects(1, &scissorRect_)` でウィンドウ全体と同じ矩形を設定している。

---

## Transform
オブジェクトの位置・回転・スケールをまとめた変換情報のこと。
この3つからワールド行列（`MakeAffineMatrix`）を作り、頂点シェーダーに渡す。

自分のコードでは `Transform` 構造体に `translation`・`rotation`・`scale` の3つが入っている。

---

## Material
オブジェクトの色・テクスチャなど見た目に関するパラメータのまとまりのこと。
DirectX の仕様ではなく、エンジン設計上の概念。

自分のコードでは `Vector4` の色データを `materialResource_` に格納してシェーダーに渡している。

---

## Scene
ゲーム内の1画面分のオブジェクト・カメラ・ライトなどをまとめた単位のこと。
「タイトル画面」「ゲーム画面」「リザルト画面」などがそれぞれ1シーンになる。
DirectX の仕様ではなく、エンジン設計上の概念。

自分のプロジェクトでは今後 `Game` クラスが実質的なゲームシーンの役割を担う予定。

---

## Log
プログラムの動作状況を記録・出力する仕組みのこと。
バグの調査・実行状況の確認に使う。DirectX のエラーコードも Log に出力することで原因を特定しやすくなる。

自分のコードでは `Logger::Log()` がこれで、全クラスのエラーや初期化完了メッセージを出力している。

---

## ImGui
ゲームやツールの開発中に使う即時描画型の GUI ライブラリのこと。
コードだけでスライダー・テキスト・ボタンを作れる。リリース版では外す。

自分のコードでは FPS 表示・カメラ操作説明・パーティクルのパラメータ調整 UI に使っている。

---

## PIX
Microsoft が提供する DirectX のデバッグ・プロファイリングツールのこと。
1フレームの描画命令をキャプチャして、どの DrawCall が重いか・GPU の使用状況を可視化できる。
パフォーマンス改善時に使う。
