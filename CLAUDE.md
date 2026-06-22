# DirectXGame - 自作ゲームエンジン

## プロジェクト概要

DirectX12 を使った自作ゲームエンジン。最終目標は **Race the Sun** クローンの制作。
「なぜ動くのかを理解しながら作る」が基本方針。ブラックボックスを極力排除する。


## 最終目標

**Race the Sun** クローン
- 太陽光で動く機体が自動前進
- 左右操作のみ
- 障害物回避
- 太陽が沈むとゲームオーバー
- 低ポリゴン・幾何学的ビジュアル

---

## 現在のアーキテクチャ

```
Engine          ← ゲームループ・初期化（ゲームロジックも混在中）
DirectXManager  ← DirectX全般（God Class、要分割）
  ├─ Triangle   ← 三角形描画（IDrawable継承）
  ├─ Line       ← ライン描画（IDrawable継承）
  ├─ Pipline    ← PSO管理（タイポあり）
  ├─ LinePipline
  ├─ DescriptorHeaps
  ├─ TextureManager
  └─ ShaderCompiler
Camera          ← ビュー・プロジェクション行列
InputDevice     ← DirectInput（キーボード・マウス）
Window          ← Win32ウィンドウ
Particle/
  └─ TrailParticle3D ← パーティクルシステム
```

---

## 修正済み

- `Triangle::kMaxInstanceCount` を `1048576` → `4096` に修正
  - 根拠: kMaxTriangles(20) × maxParticles(100) + 2 = 最大2002。4096は余裕を持たせた値
  - `public static constexpr` に変更して DirectXManager からアクセス可能に
  - `DrawTriangleRender` に上限チェック追加（超えたらスキップ＆ログ）

- **Task 7完了**: `TrailParticle3D` を `Particle/` フォルダに移動
  - `TrailParticle3D.h/.cpp` をプロジェクトルートから `Particle/` へ移動
  - `Engine/Engine.h` のインクルードパスを `"../Particle/TrailParticle3D.h"` に修正

---

# エンジン修正ロードマップ（自分で実装する）

## Task 1: FPS・フレームタイム計測を ImGui に追加

**なぜやるか**
現状 FPS が全く見えない。何ms かかっているかわからない状態で最適化しても意味がない。
測定なき最適化は無意味 → まず計測できる状態にする。

**どのファイルを触るか**
`Engine/Engine.cpp` の `DrawImGui()` 関数

**何を書くか**
`DrawImGui()` の先頭に ImGui ウィンドウを1つ追加する。
`ImGui::GetIO().Framerate` は ImGui が内部で計測している FPS。
`1000.0f / Framerate` でミリ秒に変換できる。

```cpp
ImGui::Begin("Perf");
ImGui::Text("FPS       : %.1f", ImGui::GetIO().Framerate);
ImGui::Text("FrameTime : %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
ImGui::End();
```

**確認方法**
実行して "Perf" ウィンドウに数値が出ればOK。
パーティクルを増やしたり減らしたりして FPS が変動することを確認する。

---

## Task 2: グリッドを静的バッファ化（202 DrawCall → 1 DrawCall）

**なぜやるか**
`DrawGrid()` は毎フレーム同じ202本の線を個別に DrawCall している。
グリッドは動かないので、初期化時に1つのバッファに全頂点を書いておけば
毎フレーム1回の DrawCall で済む。

**現状の問題**
`Engine/Engine.cpp` の `DrawGrid()` を見ると:
```cpp
for (float x = -50.0f; x <= 50.0f; x += 1.0f) // 101回
    DirectX_.DrawLineRender(...);               // 毎フレームDrawCall
for (float z = -50.0f; z <= 50.0f; z += 1.0f) // 101回
    DirectX_.DrawLineRender(...);               // 毎フレームDrawCall
```
合計202回/フレームのDrawCallが発生している。

**どのファイルを触るか**
- `Graphics/Object/Line/Line.h` と `Line.cpp` に静的グリッド用のメソッドを追加
- または `Engine/Engine.h` と `Engine.cpp` にグリッドバッファ用メンバを追加

**やること（手順）**
1. `Engine.h` に静的グリッド用頂点バッファのメンバを追加
2. `Engine::Initialize()` の中でグリッドの全頂点を1度だけ計算・バッファに書き込む
3. `DrawGrid()` を毎フレーム个別DrawCallではなく、バッファを1回Drawするだけに変える
4. `DrawGrid()` の for ループを削除する

**ポイント（なぜ1回で済むか）**
GPUは「この頂点データをこのやり方で描け」という命令を受け取る。
頂点データ（グリッドの線の両端座標）が変わらないなら、毎フレーム同じデータを送り直す必要はない。
初期化時に1度だけGPUメモリに書いておけば、毎フレームは「このバッファを使って描け」と言うだけでいい。

---

## Task 3: 毎DrawCallの SetPipelineState / RSSetViewports 重複排除

**なぜやるか**
`Triangle::SetPipelineCommands()` を見ると、三角形を1個描くたびに:
```cpp
commandList->RSSetViewports(1, &viewport_);       // ウィンドウサイズは変わらないのに毎回
commandList->RSSetScissorRects(1, &scissorRect_); // 毎回
commandList->SetGraphicsRootSignature(...);        // パイプラインは変わらないのに毎回
commandList->SetPipelineState(...);               // 毎回
```
これが2000回呼ばれる。ウィンドウサイズもパイプラインも変わっていないのに毎回設定し直している。
GPUのステートマシンは「前回と同じ設定」を覚えているので、変わらないものは1回だけ設定すれば十分。

**どのファイルを触るか**
- `Graphics/DirectXManager.cpp` の `BeginFrame()` に移動させる
- `Graphics/Object/Triangle/Triangle.cpp` の `SetPipelineCommands()` から重複を削除
- `Graphics/Object/Line/Line.cpp` の `SetPipelineCommands()` からも同様に削除

**やること（手順）**
1. `DirectXManager::BeginFrame()` の末尾に、TriangleとLine共通の設定を追加する
   - RSSetViewports、RSSetScissorRects はここで1回だけ
2. `Triangle::SetPipelineCommands()` から RSSetViewports / RSSetScissorRects を削除
3. `Line::SetPipelineCommands()` からも同様に削除
4. パイプラインの切り替え（Triangle→Line）はまだ必要なので SetGraphicsRootSignature と
   SetPipelineState は残す（ただし三角形の2000回ループは1回にまとめられる）

---

## Task 4: DeltaTimer クラスの追加

**なぜやるか**
現状 `Engine::Update()` の中に:
```cpp
transform1_.rotation.y += 1.0f; // FPSが60なら60度/秒、30なら30度/秒
```
FPSが変わると回転速度が変わってしまう。ゲームの動きがFPSに依存している。

**正しい考え方**
「1フレームで何度回転」ではなく「1秒で何度回転」にする。
前のフレームから何秒経過したか（デルタタイム）を計測して掛け算する:
```cpp
transform1_.rotation.y += 60.0f * deltaTime; // 常に60度/秒
```

**どのファイルを触るか（新規作成）**
`Utils/DeltaTimer.h` と `Utils/DeltaTimer.cpp` を新規作成する

**DeltaTimer の中身**
```cpp
// DeltaTimer.h
class DeltaTimer {
public:
    void Start();            // 初期化時に呼ぶ
    void Update();           // 毎フレーム先頭で呼ぶ
    float GetDeltaTime() const; // 前フレームからの経過秒数を返す
private:
    LARGE_INTEGER frequency_; // QueryPerformanceFrequency で取得
    LARGE_INTEGER lastTime_;  // 前フレームのカウンター値
    float deltaTime_ = 0.0f;
};
```

**QueryPerformanceCounter とは**
Windows が提供する高精度タイマー。CPUのクロックを使って計測するのでナノ秒単位の精度がある。
- `QueryPerformanceFrequency()` : 1秒あたりのカウント数を取得（一度だけ呼ぶ）
- `QueryPerformanceCounter()` : 現在のカウント値を取得（毎フレーム呼ぶ）
- `deltaTime = (現在 - 前回) / 周波数` で秒数に変換できる

**Engine.cpp での使い方**
```cpp
// Initialize() の末尾
deltaTimer_.Start();

// Run() のループ先頭
deltaTimer_.Update();
float dt = deltaTimer_.GetDeltaTime();

// Update() の中
transform1_.rotation.y += 60.0f * dt;
```

---

## Task 5: Camera の二重管理を解消

**なぜやるか**
現状 `Engine.h` に `CameraData cameraData_` と `Camera camera_` の両方がある。
毎フレーム `camera_.SetPosition(cameraData_.position)` で上書きしている。
Camera クラスが自分の状態を持っているのに、Engine でも同じ状態を別に保持している。
カメラの「状態の持ち主」が曖昧になっている。

**どのファイルを触るか**
- `Engine/Engine.h`
- `Engine/Engine.cpp`
- `Camera/Camera.h`
- `Camera/Camera.cpp`

**やること（手順）**
1. `Camera.h` に入力受け付け・更新のメソッドを追加する
   ```cpp
   void HandleInput();  // マウス・キーボードからカメラを動かす
   void Update();       // 毎フレームの更新
   ```
2. `CameraControl()` のロジックを `Camera::HandleInput()` に移す
3. `Engine.h` から `CameraData cameraData_` を削除
4. `Engine.cpp` の `CameraControl()` 呼び出しを `camera_.HandleInput()` に変える
5. `camera_.SetPosition()` / `SetRotation()` / `SetFov()` の3行を削除

**ポイント**
Camera クラスが自分の位置・回転・Fovを自分で管理するようになる。
Engine はカメラに「更新しろ」と言うだけでいい。

---

## Task 6: タイポ修正 Pipline → Pipeline、LinePipline → LinePipeline

**なぜやるか**
タイポ（スペルミス）はコードの読みやすさを下げ、将来の自分や他人が混乱する。
`Pipeline` が正しいスペル。

**どのファイルを触るか**
- `Graphics/Pipline/Pipline.h` → `Graphics/Pipeline/Pipeline.h` にリネーム
- `Graphics/Pipline/Pipline.cpp` → `Graphics/Pipeline/Pipeline.cpp` にリネーム
- `Graphics/Pipline/LinePipline.h` → `Graphics/Pipeline/LinePipeline.h`
- `Graphics/Pipline/LinePipline.cpp` → `Graphics/Pipeline/LinePipeline.cpp`
- `Graphics/DirectXManager.h` のインクルードパスを修正
- `Graphics/DirectXManager.cpp` の参照を修正

**やること（手順）**
1. Visual Studio のソリューションエクスプローラーでファイル名を変更（Rename）
2. クラス名を `Pipline` → `Pipeline`、`LinePipline` → `LinePipeline` に変更
3. `DirectXManager.h` の `#include` パスを修正
4. `DirectXManager.h` のメンバ変数名も修正
   ```cpp
   Pipeline pipeline_;      // pipline_ → pipeline_
   LinePipeline linePipeline_; // linePipline_ → linePipeline_
   ```
5. ビルドしてエラーがないか確認

---

## Task 7: TrailParticle3D を Particle/ フォルダに移動

**なぜやるか**
`TrailParticle3D.h` と `.cpp` がプロジェクトルートに置かれている。
他のクラスはすべて機能別フォルダにあるのに、ここだけ例外になっている。
将来パーティクルの種類が増えたときに整理されていないと困る。

**どのファイルを触るか**
- `TrailParticle3D.h` → `Particle/TrailParticle3D.h` に移動
- `TrailParticle3D.cpp` → `Particle/TrailParticle3D.cpp` に移動
- `Engine/Engine.h` のインクルードパスを修正
  ```cpp
  #include "../Particle/TrailParticle3D.h"
  ```

---

## Task 8: Engine/ フォルダへの完全集約（✅ 完了）

**実装内容**
- `Camera/` → `Engine/Camera/`
- `Window/` → `Engine/Window/`
- `InputDevice/` → `Engine/InputDevice/`
- `Graphics/` → `Engine/Graphics/`（Engine/Graphics/Object/Triangle, Line, その他サブフォルダを含む）
- `Utils/` → `Engine/Utils/`（DeltaTime, Logger, StringUtils）

**修正したインクルードパス**
1. `Engine/Engine.h`: `../Camera/` → `Camera/`, `../Window/` → `Window/`, `../InputDevice/` → `InputDevice/`, `../Graphics/` → `Graphics/`, `../Utils/` → `Utils/`
2. `Engine/Camera/Camera.h`: `../Math/` → `../../Math/`
3. `Engine/Camera/Camera.cpp`: コメント追加（同階層パスの確認）
4. `Engine/Window/Window.cpp`: `../Externals/` → `../../Externals/`
5. `Engine/Graphics/DirectXManager.h`: すべてのインクルードを `../../` ベースに統一
6. `Engine/Graphics/DirectXManager.cpp`: `../Utils/` → `../Utils/`, `../Math/` → `../../Math/`
7. `Engine/Graphics/DescriptorHeaps/DescriptorHeaps.cpp`: `../../Utils/`（既に正しい）
8. `Engine/Graphics/ShaderCompiler/ShaderCompiler.cpp`: `../../Utils/`, `../../Utils/StringUtils.h`（既に正しい）
9. `Engine/Graphics/Pipeline/Pipeline.cpp`: `../../Utils/`（既に正しい）
10. `Engine/Graphics/Pipeline/LinePipeline.cpp`: `../../Utils/`（既に正しい）
11. `Engine/Graphics/Texture/TextureManager.h/cpp`: `../../Utils/`, `../../Externals/`
12. `Engine/Graphics/Object/Triangle/Triangle.h`: `../../../Math/` → `../../../../Math/`
13. `Engine/Graphics/Object/Line/Line.h`: `../../../Math/` → `../../../../Math/`
14. `Externals/imgui/imguiManager.h`: `../../Graphics/` → `../../Engine/Graphics/`

**ビルド結果**
✅ ビルド成功（エラー 0、警告 8 のみ）

**元のフォルダのクリーンアップ**
✅ ルート配下の `Camera/`, `Window/`, `InputDevice/`, `Graphics/`, `Utils/` を削除

---

## Task 9: Engine と Game クラスの分離（✅ 完了）

**実装内容**
Engine と Game を完全に分離し、各クラスが単一の責務を持つようにしました。

**作成・修正したファイル**
1. **新規作成**
   - `Game/Game.h`: ゲーム固有のメンバと処理
   - `Game/Game.cpp`: Initialize, Update, Render の実装

2. **修正**
   - `Engine/Engine.h`: Game メンバ追加、ゲーム固有データ削除
   - `Engine/Engine.cpp`: Run()を簡潔化、Update/Render削除、Initialize()をGame初期化に
   - `Engine/Graphics/DirectXManager.h`: GetClientWidth/GetClientHeight ゲッター追加
   - `Particle/TrailParticle3D.h`: Update シグネチャ変更（deltaTime パラメータ追加）
   - `Particle/TrailParticle3D.cpp`: Update 実装修正
   - `DirectXGame.vcxproj`: Game.cpp/.h をプロジェクトに追加

**分離結果**

Engine（エンジン基盤）
```cpp
Window window_           // ウィンドウ管理
DirectXManager directX_  // DirectX管理
Camera camera_           // カメラ管理
ImGuiManager imgui_      // UI管理
DeltaTime deltaTime_     // タイマー管理
Game game_               // ゲーム処理への委譲

Initialize()  // エンジン基盤の初期化 → Game::Initialize を呼ぶ
Run()         // ゲームループ → Game::Update/Render を呼ぶ
Finalize()    // エンジン基盤の終了
```

Game（ゲーム固有）
```cpp
Transform transform1_
Transform transform2_
TrailParticle3D trailParticles_[20]
TrailParticleParameter trailParam_
TextureID textureID_

Initialize(DirectXManager*, Camera*)  // ゲーム初期化
Update(float deltaTime)               // ゲームロジック更新
Render()                              // ゲーム描画
DrawGrid()                            // グリッド描画（Game専用）
DrawImGui()                           // ゲーム UI（Trail Settings）
```

**Engine::Run() の流れ**
```cpp
void Engine::Run() {
    while (window_.ProcessMessage()) {
        InputDevice::GetInstance().Update();
        deltaTimer_.Update();
        directX_.BeginFrame();
        imgui_.BeginFrame();
        game_.Update(deltaTime_.GetDeltaTime());  // ← Game に任せる
        game_.Render();                            // ← Game に任せる
        imgui_.EndFrame(&directX_);
        directX_.EndFrame();
    }
}
```

**ビルド結果**
✅ ビルド成功（エラー 0、警告 6 のみ）
✅ DirectXGame.exe 生成（6.42MB）

---

## Task 10: Sprite を IDrawable に統合（Sprite クラス作成）

**なぜやるか**
現状 Sprite（スプライト）の描画は `DirectXManager` が直接 commandList に書き込んでいる。
Triangle や Line は `IDrawable` を継承した独立クラスになっているのに、
Sprite だけ旧体系のまま DirectXManager に埋め込まれている。
一貫性がなく、Sprite の描画コードが DirectXManager に混じって読みにくい。

**どのファイルを触るか（新規作成）**
- `Graphics/Object/Sprite/Sprite.h`
- `Graphics/Object/Sprite/Sprite.cpp`

**Sprite クラスの構造（Triangle を参考に）**
```
Sprite : IDrawable
├─ Initialize(device, textureManager, rootSignature, pipelineState)
├─ SetWvpMatrix(matrix)
├─ SetColor(color)
├─ SetViewportAndScissorRect(width, height)
├─ SetPipelineCommands(commandList, textureManager, textureID)
└─ Draw(commandList, wvpIndex)  ← IDrawable の override
```

**DirectXManager から削除するもの**
Sprite クラスができたら `DirectXManager` の以下を削除する:
- `vertexResourceSprite_`
- `vertexBufferViewSprite_`
- `transformationMatrixResourceSprite_`
- `transformationMatrixDataSprite_`
- `CreateVertexSpriteResource()`
- `SetVertexSpriteResource()`
- `CreateVertexTransformMatrixResource()`
- `SetPipelineCommands()`（Sprite用のもの）
- `RecordDrawCommands()`

---

## Task 11: Sphere を IDrawable に統合（Sphere クラス作成）

**なぜやるか**
Sprite と同じ理由。`CreateDrawSphereResource()` という名前からして設計が中途半端で、
毎フレーム GPU バッファを CreateBufferResource で確保し直している（非常に重い）。

**どのファイルを触るか（新規作成）**
- `Graphics/Object/Sphere/Sphere.h`
- `Graphics/Object/Sphere/Sphere.cpp`

**Sphere クラスの構造**
```
Sphere : IDrawable
├─ Initialize(device, textureManager, rootSignature, pipelineState, subdivision)
│   └─ 頂点データをここで1度だけ生成・GPUに転送
├─ SetWvpMatrix(matrix, wvpIndex)
├─ SetViewportAndScissorRect(width, height)
├─ SetPipelineCommands(commandList, textureManager, textureID)
└─ Draw(commandList, wvpIndex)
```

**なぜ Initialize で頂点を生成するか**
現状 `CreateDrawSphereResource()` は毎フレーム呼ばれるたびに `CreateBufferResource()` で
GPUバッファを確保し直している。球の形は変わらないのに毎フレーム確保・解放しているのは無駄。
Initialize で1回だけ作れば毎フレームは Draw するだけでいい。

---

## Task 12: DescriptorHeaps の GetTextureSrvHandle 新旧API 統合

**なぜやるか**
`DescriptorHeaps.h` に以下が混在している:
```cpp
GetTextureSrvHandle()    // 古いAPI（何番目か不明）
GetTextureSrvHandle2()   // 古いAPI（2って何？）
GetTextureSrvHandleByIndex(uint32_t) // 新しいAPI
```
古いものと新しいものが両立しているので、どれを使えばいいか読んだ人が迷う。

**やること**
1. どこで `GetTextureSrvHandle()` と `GetTextureSrvHandle2()` が使われているか調べる
2. すべて `GetTextureSrvHandleByIndex()` に置き換える
3. 古い2つのメソッドとメンバ変数 `textureSrvHandle_` / `textureSrvHandle2_` を削除

---

## Task 13: TextureManager::CreateBufferResource の責務整理

**なぜやるか**
`TextureManager::CreateBufferResource()` はテクスチャとは無関係な
頂点バッファや WVP バッファの確保にも使われている。
名前から「テクスチャ管理クラスが頂点バッファを作る」のは責務として不自然。

**やること（段階的に）**
1. `CreateBufferResource()` を `TextureManager` から切り出して独立した関数にする
   場所の候補: `Graphics/ResourceFactory.h` に static 関数として置く
   ```cpp
   namespace ResourceFactory {
       ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);
   }
   ```
2. Triangle / Line / DirectXManager が使っているすべての `textureManager_->CreateBufferResource()`
   を `ResourceFactory::CreateBufferResource(device_, ...)` に置き換える
3. `TextureManager` から `CreateBufferResource()` を削除

---

## Task 14: WVP 行列管理を Triangle/Line に統一

**なぜやるか**
WVP 行列（ワールド・ビュー・プロジェクション行列）の GPU バッファが3箇所に存在している:
- `Triangle` 内: `wvpResource_`, `wvpMappedData_`
- `Line` 内: `wvpResource_`, `wvpMappedData_`
- `DirectXManager` 内: `wvpResource_`, `wvpMappedData_`（スプライト・球用）

Sprite / Sphere クラスを作った後（Task 10・11 完了後）は、
DirectXManager の WVP バッファは不要になるので削除できる。

**やること（Task 10・11 完了後）**
1. `DirectXManager` の `wvpResource_` / `wvpMappedData_` / `wvpStride_` を削除
2. `AllocateWvpIndex()` / `SetWvpMatrix()` / `GetWvpGpuAddress()` を削除
3. `CreateTransformationMatrix()` を削除
4. `kTriangleWvpIndex` / `kSphereWvpIndex` などの定数を削除

---

## Task 15: Present(1,0) + Fence ダブル同期問題の解決

**なぜやるか**
`DirectXManager::EndFrame()` で:
```cpp
swapChain_->Present(1, 0);           // ① Vsync まで待つ（16.67ms）
WaitForSingleObject(fenceEvent_, INFINITE); // ② GPU完了まで待つ
```
2つの待機が直列に並んでいる。`Present(1, 0)` は GPU の描画完了 + Vsync まで待つので、
その直後の Fence 待機は「もう終わっている GPU を再び待つ」無駄な処理になっている。

**やること**
`DirectXManager.cpp` の `EndFrame()` のフェンス待機を次フレームの先頭に移動させる。
つまり「前フレームの GPU が終わったか確認してから今フレームの記録を始める」流れにする:

```
変更前:
  ExecuteCommandLists → Present(待機) → Fence待機 → 次フレームのBeginFrame

変更後:
  ExecuteCommandLists → Present(待機) → [次フレームへ]
  BeginFrame先頭でFence確認（もう終わっているはずなので即通過）
```

`Present(1, 0)` の第1引数を `0` に変えると Vsync なし（フレームレート上限なし）になるが、
まずはダブル同期を解消してから考える。

---

## Task 16: DirectXManager を Device/Renderer/CommandQueue に分割

**なぜやるか（God Class 問題）**
`DirectXManager` が現在担っている責務:
- DirectX デバイス・ファクトリの管理
- コマンドキュー・コマンドリストの管理
- スワップチェーンの管理
- フェンス・同期の管理
- 描画（DrawTriangleRender, DrawLineRender...）
- リソース確保

1クラスに責務が集中しすぎている。変更のたびに全員に影響が出る。

**分割案**
```
DirectXDevice      ← デバイス・アダプタ・ファクトリ・フェンス
CommandManager     ← コマンドキュー・アロケータ・コマンドリスト
SwapChainManager   ← スワップチェーン・バックバッファ・RTV
Renderer           ← BeginFrame/EndFrame、DrawXxx の呼び出し
                     （Device/Command/SwapChain を内包）
```

**注意**: これは最も大きいリファクタリング。Task 10・11・14 が完了してから行う。
`DirectXManager` の不要なメンバが減った状態でやると作業量が大幅に減る。

---

## Task 17: Camera タイポ修正 `GetAspeRatio` → `GetAspectRatio`

**なぜやるか**
Task 6 で `Pipline` → `Pipeline` を直したのと同じ理由。
`Aspect` のスペルミスで `e` が抜けている。呼び出し元も含めて統一する。

**どのファイルを触るか**
- `Engine/Camera/Camera.h` のメソッド宣言
- `Engine/Camera/Camera.cpp` の実装
- `Engine/Engine.cpp` の呼び出し箇所（`GetAspeRatio` → `GetAspectRatio`）

---

## Task 18: `TrailParticle3D::Update` に deltaTime を追加

**なぜやるか**
Task 4 で Engine の transform 更新は FPS 非依存にしたが、
`TrailParticle3D::Update()` は deltaTime を受け取っていないため、
パーティクルの速度・寿命タイマーがまだ FPS に依存している。
60FPS なら 60 回/秒、30FPS なら 30 回/秒呼ばれ、速度が変わってしまう。

**どのファイルを触るか**
- `Particle/TrailParticle3D.h` のシグネチャ変更
- `Particle/TrailParticle3D.cpp` の内部ロジック修正
- `Engine/Engine.cpp` の呼び出し元に deltaTime を渡す

**変更内容**
```cpp
// Before
void Update(Vector3& outTranslation, Vector3& outRotation);

// After
void Update(const Vector3& pos, const Vector3& rotation, float deltaTime);
```
第1・第2引数も `const Vector3&` に変更する。
パーティクルは渡された pos/rotation を「読む」だけで、呼び出し元の値を書き換えるべきではない。
内部の `lifeTimer` や速度の更新に `× deltaTime` を掛ける。

---

## Task 19: `Camera::Update()` の整理

**なぜやるか**
Task 5 以降、`Camera::HandleInput()` が内部で状態を更新しているため
`Camera::Update()` は現在どこからも呼ばれていない（宣言だけ存在する空のメソッド）。
使われていないメソッドが残っていると、読んだ人が「どこかで呼ばれているのか？」と混乱する。

**やること**
1. `Camera.h` と `Camera.cpp` から `Update()` を削除する
2. ビルドしてエラーがないか確認

**注意**
将来 Game クラスがカメラを制御する必要が出たとき（プレイヤー追従など）は、
`HandleInput()` とは別に `SetPosition()` / `SetRotation()` を使えばよい。
現時点で未使用のメソッドは残さない。

---

# Race the Sun 実装ロードマップ（エンジン修正完了後）

## Phase 1: プロトタイプ（機体前進＋衝突）

追加クラス:
- `Game/Player.h/.cpp`
- `Physics/AABB.h`
- `Game/Obstacle.h/.cpp`
- `Game/ObstacleManager.h/.cpp`
- `Game/GameStateManager.h/.cpp`

## Phase 2: コアメカニクス

- `Game/SunSystem.h/.cpp` ← 太陽角度・ソーラーパワー
- アイテム収集（三角形）
- スコア（走行距離）
- 速度上昇

## Phase 3: 演出・生成

- プロシージャル障害物生成
- 地面の Quad メッシュ
- 太陽オブジェクト（三角形で表現）
- `TrailParticle3D` を機体エフェクトとして流用

---

## Triangle の重要定数

```cpp
Triangle::kMaxInstanceCount = 4096
// 根拠: kMaxTriangles(20) × maxParticles(100) + 2 = 最大2002
// パーティクル数を変えるときは kMaxTriangles × maxParticles で再計算
// この値より多く描こうとするとバッファ範囲外でクラッシュする（チェック済み）
```

---

## ビルド方法

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "DirectXGame.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

---

## コーディング方針

- **コードの書き換えは自分で行う。Claude は説明と CLAUDE.md 更新のみ。**
- 「なぜ動くのか」を理解してから実装する
- ブラックボックスを残さない
- 新機能追加より理解優先
- コメントは「なぜ」を書く（「何を」はコードで読める）
