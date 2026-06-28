# DirectXGame アーキテクチャ

## プロジェクト概要

DirectX12 自作ゲームエンジン。最終目標は Race the Sun クローン。

---

## ディレクトリ構成

```
DirectXGame/
├── main.cpp                        ← エントリーポイント
├── Math/
│   ├── MathTypes.h                 ← Vector2/3/4, Matrix4x4, Transform
│   ├── VectorMath.h                ← ベクトル演算
│   ├── MatrixMath.h                ← 行列演算
│   ├── TransformMath.h             ← アフィン変換・ビュー行列
│   └── Collision.h/.cpp            ← 当たり判定（AABB/OBB/Sphere/Ray等）
├── Debug/
│   └── Debug.h/.cpp                ← クラッシュハンドラ・デバッグレイヤー
├── Engine/
│   ├── Engine.h/.cpp               ← ゲームループ統合
│   ├── Camera/
│   │   └── Camera.h/.cpp           ← ビュー・プロジェクション行列・入力
│   ├── InputDevice/
│   │   └── InputDevice.h/.cpp      ← DirectInput シングルトン
│   ├── Window/
│   │   └── Window.h/.cpp           ← Win32 ウィンドウ
│   ├── Utils/
│   │   ├── DeltaTime.h/.cpp        ← フレーム時間計測
│   │   ├── Logger.h/.cpp           ← ログ出力
│   │   └── StringUtils.h/.cpp      ← 文字列変換
│   └── Graphics/
│       ├── Renderer/
│       │   ├── DirectXManager.h/.cpp ← DirectX12 基盤（Device/Command/SwapChain）
│       │   └── Renderer.h/.cpp       ← 描画コマンド蓄積・Flush
│       ├── Pipeline/
│       │   ├── Pipeline.h/.cpp       ← Triangle/Cube/Sphere 用 PSO
│       │   └── LinePipeline.h/.cpp   ← Line 用 PSO
│       ├── DescriptorHeaps/
│       │   └── DescriptorHeaps.h/.cpp ← RTV/DSV/SRV ヒープ管理
│       ├── Texture/
│       │   └── TextureManager.h/.cpp  ← テクスチャロード・キャッシュ
│       ├── ShaderCompiler/
│       │   └── ShaderCompiler.h/.cpp  ← HLSL コンパイル (DXC)
│       ├── ResourceFactory/
│       │   └── ResourceFactory.h/.cpp ← GPU リソース生成ヘルパー
│       ├── DirectXDevice/
│       │   └── DirectXDevice.h/.cpp   ← ID3D12Device・フェンス
│       ├── CommandManager/
│       │   └── CommandManager.h/.cpp  ← コマンドリスト・アロケーター
│       ├── SwapChainManager/
│       │   └── SwapChainManager.h/.cpp ← スワップチェーン・Present
│       └── Object/
│           ├── IDrawable.h             ← 描画オブジェクト基底インターフェース
│           ├── Triangle/Triangle.h/.cpp ← 正四面体（インスタンシング対応）
│           ├── Cube/Cube.h/.cpp         ← 立方体（インスタンシング対応）
│           ├── Line/Line.h/.cpp         ← ライン描画
│           ├── Sprite/Sprite.h/.cpp     ← スプライト
│           └── Sphere/Sphere.h/.cpp     ← 球体
├── Game/
│   └── Game.h/.cpp                 ← ゲームロジック
├── Particle/
│   └── TrailParticle3D.h/.cpp      ← 軌跡パーティクルシステム
├── HLSL/
│   ├── Object3D.VS.hlsl            ← 頂点シェーダー（インスタンシング対応）
│   ├── Object3D.PS.hlsl            ← ピクセルシェーダー
│   ├── Object3d.hlsli              ← 共通構造体定義
│   └── ...                         ← Line/Sprite 用シェーダー
└── Resources/
    ├── White.png                   ← デフォルトテクスチャ（TextureHandle=0）
    └── ...
```

---

## クラス依存関係

```
main.cpp
  ├── Engine
  │   ├── Window
  │   ├── DirectXManager
  │   │   ├── DirectXDevice      (ID3D12Device 所有)
  │   │   ├── CommandManager     (CommandList/Allocator 所有)
  │   │   └── SwapChainManager   (SwapChain/RenderTarget 所有)
  │   ├── Renderer
  │   │   ├── ShaderCompiler
  │   │   ├── Pipeline           (Triangle/Cube/Sphere 用 PSO)
  │   │   ├── LinePipeline       (Line 用 PSO)
  │   │   ├── TextureManager     (テクスチャキャッシュ)
  │   │   ├── Triangle           (kMaxInstanceCount=4096)
  │   │   ├── Cube               (kMaxInstanceCount=4096)
  │   │   ├── Line
  │   │   ├── Sprite
  │   │   └── Sphere
  │   ├── Camera
  │   ├── InputDevice            (シングルトン)
  │   ├── ImGuiManager
  │   └── DeltaTime
  └── Game
      ├── Renderer*              (借用)
      ├── Camera*                (借用)
      └── TrailParticle3D[20]
```

---

## フレームの流れ

```
┌─────────────────────────────────────────────────────────┐
│  Engine::Update()                                       │
│    ├─ window_.ProcessMessage()   ウィンドウメッセージ    │
│    ├─ InputDevice::Update()      キー・マウス状態更新    │
│    ├─ deltaTime_.Update()        フレーム時間更新        │
│    ├─ directX_.BeginFrame()      コマンドリストをOpen    │
│    ├─ renderer_.ResetFrameIndex()                       │
│    └─ imgui_.BeginFrame()                               │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  Game::Update(deltaTime)                                │
│    ├─ transform の回転更新                               │
│    ├─ camera_->HandleInput()     カメラ操作             │
│    └─ TrailParticle3D[20]::Update()  パーティクル更新   │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  Game::Render()                                         │
│    ├─ view / projection 行列を取得                       │
│    ├─ renderer_->DrawTriangle(wvp, color)  ← 登録だけ   │
│    ├─ renderer_->DrawCube(wvp, color)      ← 登録だけ   │
│    ├─ renderer_->DrawLine(...)             ← 登録だけ   │
│    └─ DrawImGui()                                       │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│  Engine::Flush()                                        │
│    ├─ renderer_.FlushTriangles()   TextureID でソート   │
│    │     └─ Triangle::Draw(instanceCount, startInstance)│
│    ├─ renderer_.FlushCubes()                            │
│    ├─ renderer_.FlushLines()                            │
│    ├─ renderer_.FlushSpheres()                          │
│    ├─ imgui_.EndFrame()                                 │
│    └─ directX_.EndFrame()                               │
│          ├─ CommandList::Close()                        │
│          ├─ ExecuteCommandList()  GPU に送る            │
│          ├─ Signal() / WaitForFence()                   │
│          ├─ Present()            画面に表示             │
│          └─ ResetCommandList()   次フレームの準備       │
└─────────────────────────────────────────────────────────┘
```

---

## 描画の仕組み（GPU インスタンシング）

```
Game::Render() で DrawTriangle を 2000 回呼ぶ
  ↓
Renderer::triangleCommands_ に蓄積（登録だけ）
  ↓
Engine::Flush() → Renderer::FlushTriangles()
  ↓
TextureHandle でソート（SetPipelineCommands の切り替えを最小化）
  ↓
WVP 行列・色を StructuredBuffer に一括書き込み
  ↓
Triangle::Draw(commandList, instanceCount=2000, startInstance=0)
  └─ DrawInstanced(頂点数=12, インスタンス数=2000, 0, 0)
       GPU が SV_InstanceID で各インスタンスの WVP/色を参照
       → 1回の DrawCall で 2000 個を並列描画
```

### ルートシグネチャ（Triangle/Cube 共通）

| スロット | レジスタ | 内容 | 可視性 |
|---------|---------|------|--------|
| [0] | t0 | テクスチャ SRV | PS |
| [1] | t1 | WVP 行列配列 StructuredBuffer | VS |
| [2] | t2 | 色配列 StructuredBuffer | VS |

---

## テクスチャ管理

```cpp
// ゲーム側でロード → TextureHandle を受け取る
TextureHandle tex = renderer_->LoadTexture("Resources/grass.png");

// 描画時にハンドルを渡す
renderer_->DrawCube(wvp, color, tex);       // テクスチャあり
renderer_->DrawTriangle(wvp, color);        // テクスチャなし（省略=kTextureNone=白）
```

- `TextureHandle` は `uint32_t` の別名
- `kTextureNone = 0` は白テクスチャ（エンジン起動時に自動登録）
- 同じパスを渡しても二重ロードしない

---

## 当たり判定（Collision）

`Math/Collision.h` をインクルードするだけで使える。

```cpp
#include "../Math/Collision.h"

// AABB 同士
AABB a = { {-0.5f,-0.5f,-0.5f}, {0.5f,0.5f,0.5f} };
AABB b = { {0.4f,-0.5f,-0.5f}, {1.4f,0.5f,0.5f} };
if (Collision::AABBAABB(a, b)) { /* ヒット */ }

// 球同士
Sphere s1 = { {0,0,0}, 1.0f };
Sphere s2 = { {1.5f,0,0}, 1.0f };
if (Collision::SphereSphere(s1, s2)) { /* ヒット */ }
```

対応している判定一覧：

| | Sphere | Plane | Segment | Line | Ray |
|---|---|---|---|---|---|
| **Sphere** | ✓ | ✓ | - | - | - |
| **AABB** | ✓ | - | ✓ | ✓ | ✓ |
| **OBB** | ✓ | - | ✓ | ✓ | ✓ |
| **Triangle** | - | - | ✓ | ✓ | ✓ |
| **OBB vs OBB** | ✓ | | | | |

---

## 重要な定数

| 定数 | 値 | 場所 |
|------|-----|------|
| `Triangle::kMaxInstanceCount` | 4096 | Triangle.h |
| `Cube::kMaxInstanceCount` | 4096 | Cube.h |
| `Game::kMaxTriangles` | 20 | Game.h |
| `TrailParticleParameter::maxParticles` | 100 | TrailParticle3D.h |
| 最大パーティクル総数 | 20 × 100 = 2000 | 計算値 |
| ウィンドウサイズ | 1280 × 720 | main.cpp |
| カメラ初期位置 | (0, 0.5, -5) | Engine.cpp |
| SRV heapIndex: Triangle WVP | 10 | Triangle.cpp |
| SRV heapIndex: Triangle Color | 11 | Triangle.cpp |
| SRV heapIndex: Cube WVP | 12 | Cube.cpp |
| SRV heapIndex: Cube Color | 13 | Cube.cpp |
