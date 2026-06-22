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

## Task 1: FPS・フレームタイム計測を ImGui に追加（✅ 完了）

**実装状況**
✅ `Game/Game.cpp` の `DrawImGui()` で実装完了
- `ImGui::Begin("FPS")` で FPS ウィンドウ表示
- `ImGui::GetIO().Framerate` で FPS 値取得
- frameTime 表示も実装済み

---

## Task 2: グリッドを静的バッファ化（202 DrawCall → 1 DrawCall）（✅ 完了）

**実装状況**
✅ DirectXManager に `InitializeGridLines()` と `DrawGridBatch()` で実装完了
- `InitializeGridLines()`: 初期化時に全グリッド頂点を 1 度だけ GPU に書き込み
- `DrawGridBatch()`: 毎フレーム 1 回の DrawCall で全グリッドを描画
- 効果: 202 DrawCall → 1 DrawCall に削減

---

## Task 3: 毎DrawCallの SetPipelineState / RSSetViewports 重複排除（✅ 完了）

**実装状況**
✅ 完全実装
- `DirectXManager::BeginFrame()` で RSSetViewports と RSSetScissorRects を 1 回設定（170-171行）
- `DrawTriangleRender()` から `SetViewportAndScissorRect()` 呼び出しを削除（重複排除）
- `DrawLineRender()` から `SetViewportAndScissorRect()` 呼び出しを削除（重複排除）
- `DrawGridBatch()` から `SetViewportAndScissorRect()` 呼び出しを削除（重複排除）
- 効果: 毎フレームの不要な Viewport/ScissorRect 設定が 2000+ 回から 1 回に削減

**修正内容**
- `DirectXManager::DrawTriangleRender()`: line 568 の SetViewportAndScissorRect() を削除
- `DirectXManager::DrawLineRender()`: line 599 の SetViewportAndScissorRect() を削除
- `DirectXManager::DrawGridBatch()`: line 814 の SetViewportAndScissorRect() を削除
- SetPipelineCommands は保持（TextureID 設定が必要）

---

## Task 4: DeltaTimer クラスの追加（✅ 完了）

**実装状況**
✅ `Engine/Utils/DeltaTimer.h/.cpp` で実装完了
- `QueryPerformanceCounter()` を使用した高精度タイマー
- `Start()`: 初期化時に周波数を取得
- `Update()`: 毎フレーム先頭でデルタタイムを計算
- `GetDeltaTime()`: 前フレームからの経過秒数を返す
- 利用: `Engine/Engine.cpp` で `deltaTime_.Update()` 後に `game_.Update(deltaTime_.GetDeltaTime())`
- 効果: ゲーム速度が FPS に依存しなくなった

---

## Task 5: Camera の二重管理を解消（✅ 完了）

**実装状況**
✅ `Camera` クラスが自身の状態を完全に管理
- `Camera::HandleInput(float deltaTime)`: マウス・キーボード入力を処理
- `Engine.h` から `CameraData cameraData_` を削除
- Engine は Camera に「入力処理をしろ」と言うだけ
- 責務の明確化: Camera が自分の状態を管理

---

## Task 6: タイポ修正 Pipline → Pipeline、LinePipline → LinePipeline（✅ 完了）

**実装状況**
✅ フォルダ・ファイル・クラス名をすべて修正
- `Engine/Graphics/Pipline/` → `Engine/Graphics/Pipeline/`
- `Pipline.h/.cpp` → `Pipeline.h/.cpp`
- `LinePipline.h/.cpp` → `LinePipeline.h/.cpp`
- `DirectXManager.h` のメンバ変数も修正
- ビルド確認完了

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

## Task 10: Sprite を IDrawable に統合（Sprite クラス作成）（✅ 完了）

**実装状況**
✅ 完了
- `Engine/Graphics/Object/Sprite/Sprite.h/.cpp` を新規作成
- `Sprite : IDrawable` として Triangle と同じ構造で実装
- ルートパラメータ: `0=Material`, `1=WVP`, `2=Texture`
- `DirectXManager` に `std::unique_ptr<Sprite> sprite_` を追加
- `DrawSpriteRender()` を `sprite_->SetWvpMatrix/SetPipelineCommands/Draw` に置き換え
- 古い Sprite 関連メンバ・メソッドを DirectXManager から完全削除
  - `vertexResourceSprite_`, `vertexBufferViewSprite_`, `transformationMatrixResourceSprite_`, `transformationMatrixDataSprite_`
  - `CreateVertexSpriteResource()`, `SetVertexSpriteResource()`, `CreateVertexTransformMatrixResource()`, `RecordDrawCommands()`

---

## Task 11: Sphere を IDrawable に統合（Sphere クラス作成）（✅ 完了）

**実装状況**
✅ 完了
- `Engine/Graphics/Object/Sphere/Sphere.h/.cpp` を新規作成
- `Sphere : IDrawable` として実装。頂点データを `Initialize()` で1度だけ生成・GPU転送
- subdivision=30, radius=1.0f で初期化（スケールは Transform で対応）
- `DirectXManager` に `std::unique_ptr<Sphere> sphere_` を追加
- `CreateDrawSphereResource()` → `DrawSphereRender()` に置き換え
- `DirectXManager::SetPipelineCommands()` を削除（各クラスが自前で持つため不要に）
- 古い Sphere 関連メンバ（`vertexResourceSphere_`, `vertexBufferViewSphere_`, `sphereVertexCount_`, `kSphereWvpIndex`）を削除

---

## Task 12: DescriptorHeaps の GetTextureSrvHandle 新旧API 統合（✅ 完了）

**実装状況**
✅ 完了
- `GetTextureSrvHandle()` / `GetTextureSrvHandle2()` はどこからも呼ばれていないことを確認
- `DescriptorHeaps.h` から両メソッドとメンバ変数 `textureSrvHandle_` / `textureSrvHandle2_` を削除
- `DescriptorHeaps.cpp` から両変数への代入を削除
- `DirectXManager.h` の `GetTextureSrvHandle()` getter も削除
- `GetTextureSrvHandleByIndex()` に統一

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

## Task 17: Camera タイポ修正 `GetAspeRatio` → `GetAspectRatio`（✅ 完了）

**実装状況**
✅ 完了
- `Engine/Camera/Camera.h`: `GetAspeRatio` → `GetAspectRatio` に修正
- `Engine/Camera/Camera.cpp`: 実装も同様に修正
- `Game/Game.cpp`: 呼び出し箇所 2 箇所を修正（L49, L75）
- 注: Engine.cpp ではなく Game.cpp が呼び出し元だった

---

## Task 18: `TrailParticle3D::Update` に deltaTime を追加（✅ 完了）

**実装状況**
✅ `Particle/TrailParticle3D.h/.cpp` で実装完了（Task 9 時に実装）
- シグネチャ変更: `Update(const Vector3& pos, const Vector3& rotation, float deltaTime)`
- `const Vector3&` で入力パラメータ化（読み取り専用）
- 内部で `currentPos` を計算し、パーティクル速度が FPS 非依存に
- `Game/Game.cpp` から `trailParticles_[i].Update(triangleTransforms_[i].translation, triangleTransforms_[i].rotation, deltaTime)` で呼び出し
- 効果: パーティクルの動きが FPS に依存しなくなった

---

## Task 19: `Camera::Update()` の整理（✅ 完了）

**実装状況**
✅ 完了
- `Camera.h` から `void Update();` の宣言を削除
- `Camera.cpp` から空の `Update()` 実装を削除
- どこからも呼ばれていないことを確認してから削除

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

---

## タスク効率判定：理解重視 vs 効率重視

すべてを「理解ベース」で書く必要はない。タスク種別と学習段階で判断する。

### 開始時チェック（3問）

| 問 | Yes → | No → |
|---|---|---|
| Q1: このスキルを3ヶ月後に応用する必要があるか？ | 理解重視 | 効率重視 |
| Q2: 本番でエラーが出た時、自分で診断・修正できる必要があるか？ | 理解重視 | 効率重視 |
| Q3: 既に似たコードを3回以上書いたか？ | 効率重視 | 理解重視 |

### タスク別ガイドライン

| タスク種別 | モード | AIの使い方 |
|-----------|--------|-----------|
| コアアーキテクチャ（PSO・バッファ・テクスチャ管理） | 理解重視 | Plan → 設計説明 → 実装 |
| バグ原因診断 | 理解重視 | 仮説 → 原因究明 → 修正 |
| ゲームロジック新規実装（プレイヤー・敵AI初版） | 理解重視 | Plan → WHY説明 → 実装 |
| 既知パターン応用（2回目以降の敵・パーティクル追加） | 混合 | 骨組みはAI → 目的部分だけ確認 |
| ボイラープレート（ImGuiパラメータ・ヘッダー定義） | 効率重視 | AI生成 → 動作確認のみ |
| デバッグツール・ユーティリティ | 効率重視 | AI生成 → さっさと使う |

### 「最低限の理解」でOKなパターン

全行を理解する必要はない。コア概念3つを5分で押さえたら実装OK。
詳細な数学・アルゴリズムは必要な時に深掘りする。

### コミット前チェック

**理解重視タスク:**
- 実装の「Why」を説明できるか
- エラー時に原因を特定できるか
- 次回、似たコードを自分で書けそうか

**効率重視タスク:**
- 動作するか
- 既存コードと衝突しないか

### エンジン層 vs ゲーム層

- **DirectX12 エンジン層**（PSO・バッファ・リソース管理）→ 理解必須
- **ゲームロジック初版**（シーン・衝突・プレイヤー）→ 理解重視
- **2体目以降の敵・追加パーティクル・UI**→ 効率重視OK

**例外**：新しい DirectX 機能や未知のエンジン拡張の学習フェーズは、一時的にすべて理解重視に切り替える。

---

## 理解を深めるための実践ガイド

### 3段階チェックリスト（理解の証拠）

タスク完了後、以下3つがYesなら「理解した」と判定。

```
【理解度判定】
□ Plan を読んで、設計意図を「自分の言葉で」説明できるか
  例：「なぜこの関数でバッファ確保するのか」→ 説明できたらOK

□ 実装中・完成後、エラーが出た時に原因を診断できるか
  例：「なぜDrawCallが増えた？」→ 原因を特定できたらOK

□ 完成後、Plan や説明を見ずに、同じ内容を書き直せるか
  例：「もう一個同じ構造の敵を作ってみて」→ できたらOK
```

3つすべてYesで、その概念は習得済み。

### 危険信号（効率重視に振りすぎ）

以下に当てはまったら、理解フェーズに戻す：

```
❌ Plan読まずに実装してしまった
❌ エラー出たけど原因確認せず「ふーん」で済ませた
❌ 2回目の同じタスクで「あ、忘れた」と思った
❌ 「とりあえず動いたからOK」で終わった
```

特に「2回目で忘れた」は要注意。パターン習得に3回必要。

### パターン習得の流れ（3回の法則）

| 実施 | 目標 | 方針 |
|------|------|------|
| **1回目** | 全体像を理解 | Plan熟読 → 実装 → 動作確認 |
| **2回目** | 細部を理解 | Plan軽く → 実装中にエラー原因を考える |
| **3回目** | 自動化（パターン化） | 説明なし → 自力で書き切る |
| **4回目以降** | 効率化 | テンプレだけ思い出す → テンプレ詰める |

3回目で「あ、これパターンだ」と気づく。ここが習得のポイント。

### 理解を確実にするための工夫

**実装中の問い習慣**

3〜5行書いたら一度止めて：
```
「ここなぜ必要？」「ここ間違ったらどうなる？」
→ Planコメント確認 or 試行錯誤
```

**異常系までテストする**

正常系だけ動いたら「理解した」ではない。

例：Triangle の `kMaxInstanceCount = 4096`
- 正常系：20個の三角形描画 ✓
- 異常系：100000個描こうとしたら？ ← ここを試す

→ 「バッファサイズの限界」が腑に落ちる。

**エラーは「学習機会」**

- エラー出た → 説明見る → 試す → 治す
- このサイクルが最も強い理解を生む
- エラーなしで完成は逆に危険（理解ズレに気づかない）

### 学習フェーズの判定

| 状況 | 対応 |
|------|------|
| 新しい概念（PSO初見、ルートシグネチャ初見） | 理解重視。時間かかってOK |
| 3回書いたパターン | 効率重視OK。ただしテスト忘れずに |
| 本番バグ対応 | 問答無用で理解重視。原因わかるまで |
| 急いでる時 | 効率重視、でも翌日「なぜ動いた」を確認 |

---

## Claude との使い方ガイド

### Claude は「説明・設計のみ」

- **Plan を作成** → 設計意図・WHY を説明
- **実装は自分で** → 手を動かす中で理解が深まる
- **エラー出た** → 原因診断は提供、修正は自分でやる

### こうすると効きやすい

```
1. Claudeから Plan をもらう
2. 10分読む（「なぜこの設計？」を腑に落とす）
3. 実装開始（ここが肝心）
4. エラー出た → 原因を自分で考える（5分考えて分からなかったら聞く）
5. 完成 → 同じ内容をもう一回書いてみる（パターン習得）
```

「楽をしてしまう」を防ぐのは、意志の問題ではなく仕組みの問題。
このプロセスで強制的に理解が深まる。
