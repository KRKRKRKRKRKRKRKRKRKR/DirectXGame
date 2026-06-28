# エンジン修正タスク一覧（Task 1-21）

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
- `DirectXManager::BeginFrame()` で RSSetViewports と RSSetScissorRects を 1 回設定
- `DrawTriangleRender()` から `SetViewportAndScissorRect()` 呼び出しを削除
- `DrawLineRender()` から `SetViewportAndScissorRect()` 呼び出しを削除
- `DrawGridBatch()` から `SetViewportAndScissorRect()` 呼び出しを削除
- 効果: 毎フレームの不要な Viewport/ScissorRect 設定が 2000+ 回から 1 回に削減

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

## Task 7: TrailParticle3D を Particle/ フォルダに移動（✅ 完了）

**実装状況**
✅ 完了
- `TrailParticle3D.h/.cpp` をプロジェクトルートから `Particle/` へ移動
- `Engine/Engine.h` のインクルードパスを修正

---

## Task 8: Engine/ フォルダへの完全集約（✅ 完了）

**実装内容**
- `Camera/` → `Engine/Camera/`
- `Window/` → `Engine/Window/`
- `InputDevice/` → `Engine/InputDevice/`
- `Graphics/` → `Engine/Graphics/`
- `Utils/` → `Engine/Utils/`

**ビルド結果**
✅ ビルド成功（エラー 0、警告 8 のみ）

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
   - `Engine/Engine.cpp`: Run()を簡潔化
   - `Engine/Graphics/DirectXManager.h`: GetClientWidth/GetClientHeight ゲッター追加
   - `Particle/TrailParticle3D.h`: Update シグネチャ変更（deltaTime パラメータ追加）

**ビルド結果**
✅ ビルド成功（エラー 0、警告 6 のみ）

---

## Task 10: Sprite を IDrawable に統合（✅ 完了）

**実装状況**
✅ 完了
- `Engine/Graphics/Object/Sprite/Sprite.h/.cpp` を新規作成
- `Sprite : IDrawable` として Triangle と同じ構造で実装
- ルートパラメータ: `0=Material`, `1=WVP`, `2=Texture`
- `DirectXManager` に `std::unique_ptr<Sprite> sprite_` を追加
- 古い Sprite 関連メンバを DirectXManager から完全削除

---

## Task 11: Sphere を IDrawable に統合（✅ 完了）

**実装状況**
✅ 完了
- `Engine/Graphics/Object/Sphere/Sphere.h/.cpp` を新規作成
- `Sphere : IDrawable` として実装
- subdivision=30, radius=1.0f で初期化
- `DirectXManager` に `std::unique_ptr<Sphere> sphere_` を追加
- 古い Sphere 関連メンバを削除

---

## Task 12: DescriptorHeaps の GetTextureSrvHandle 新旧API 統合（✅ 完了）

**実装状況**
✅ 完了
- `GetTextureSrvHandle()` / `GetTextureSrvHandle2()` をすべて削除
- `GetTextureSrvHandleByIndex()` に統一

---

## Task 13: TextureManager::CreateBufferResource の責務整理（✅ 完了）

**実装状況**
✅ 完了
- `Engine/Graphics/ResourceFactory/ResourceFactory.h/.cpp` を新規作成
- `namespace ResourceFactory { CreateBufferResource(device, size) }` として独立
- 全ファイルの `CreateBufferResource` 呼び出しを `ResourceFactory::` に置き換え

---

## Task 14: WVP 行列管理を Triangle/Line に統一（✅ 完了）

**実装状況**
✅ 完了
- `wvpResource_` / `wvpMappedData_` / `wvpStride_` / `wvpAllocatedCount_` を削除
- `AllocateWvpIndex()` / `SetWvpMatrix()` / `GetWvpGpuAddress()` を削除

---

## Task 15: Present(1,0) + Fence ダブル同期問題の解決（✅ 完了）

**実装状況**
✅ 完了

**変更内容**
- `EndFrame()`: フェンス待機・Reset を削除。Signal → Present で終わる
- `BeginFrame()`: 先頭にフェンス待機を追加
- `WaitForGPUCompletion()`: 末尾の Reset を削除
- `Finalize()`: 新規 Signal を発行してから待機に変更

---

## Task 16: DirectXManager を Device/Renderer/CommandQueue に分割

**なぜやるか（God Class 問題）**
`DirectXManager` が現在担っている責務が多すぎる：
- DirectX デバイス・ファクトリの管理
- コマンドキュー・コマンドリストの管理
- スワップチェーンの管理
- フェンス・同期の管理
- 描画（DrawTriangleRender, DrawLineRender...）
- リソース確保

**分割案**
```
DirectXDevice      ← デバイス・アダプタ・ファクトリ・フェンス
CommandManager     ← コマンドキュー・アロケータ・コマンドリスト
SwapChainManager   ← スワップチェーン・バックバッファ・RTV
Renderer           ← BeginFrame/EndFrame、DrawXxx の呼び出し
```

**注意**: これは最も大きいリファクタリング。Task 10・11・14 完了後に実施予定。

---

## Task 17: Camera タイポ修正 `GetAspeRatio` → `GetAspectRatio`（✅ 完了）

**実装状況**
✅ 完了
- `Engine/Camera/Camera.h`: `GetAspeRatio` → `GetAspectRatio` に修正
- `Engine/Camera/Camera.cpp`: 実装も同様に修正
- `Game/Game.cpp`: 呼び出し箇所 2 箇所を修正

---

## Task 18: `TrailParticle3D::Update` に deltaTime を追加（✅ 完了）

**実装状況**
✅ 完了（Task 9 時に実装）
- シグネチャ変更: `Update(const Vector3& pos, const Vector3& rotation, float deltaTime)`
- パーティクル速度が FPS 非依存に

---

## Task 19: `Camera::Update()` の整理（✅ 完了）

**実装状況**
✅ 完了
- `Camera.h` から `void Update();` の宣言を削除
- `Camera.cpp` から空の `Update()` 実装を削除

---

## Task 20: DrawBatch 実装試行と設計変更（✅ 実施 / 次フェーズ延期）

**実装試行結果**
✅ 試行実施、❌ 根本課題発見 → 次フェーズで修正設計を採用

**発見した問題点**
- `Triangle::DrawBatch()` を実装し、複数の WVP インデックスを同時に処理しようとした
- DrawInstanced 実行時に GPU リソース状態エラーが発生
- 根本原因：1 つの DrawCall 内で複数の独立した WVP インデックスを管理できない仕組みになっていた

---

## Task 21: SetPipelineCommands を 1 回に削減（✅ 完了）

**実装状況**
✅ 完了
- `Game/Game.cpp` の `Render()` を 3ステップに変更
  - Step 1: テクスチャなし (None) グループのバッファ書き込み
  - Step 2: テクスチャ付き (textureID_) グループのバッファ書き込み
  - Step 3: グループ A をバッチ描画（SetPipelineCommands 1回）
  - Step 4: グループ B をバッチ描画（SetPipelineCommands 1回）

**測定結果（PIX で計測）**
```
DrawInstanced:              784回（変わらず）
SetPipelineState:           784回 → 5回（99.4%削減）
SetGraphicsRootSignature:   784回 → 4回（99.5%削減）
```

**利点**
✅ GPU パイプライン切り替えが 784回 → 5回に削減
✅ ルートシグネチャ設定が 784回 → 4回に削減
✅ GPU キュー処理の負荷大幅削減

**ビルド・実行確認**
✅ ビルド成功（エラー 0、警告 14）
✅ ゲーム実行成功（三角形・パーティクル描画正常）
✅ PIX で SetPipelineState/SetGraphicsRootSignature の呼び出し回数激減を確認
