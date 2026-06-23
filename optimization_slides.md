# DirectX12 描画最適化：2000+ DrawCall → 数回に削減

---

## スライド 1: ビフォー（最適化前）

```
【毎フレーム】
┌─────────────────────────────────────────────────────────────┐
│                     Game::Render()                          │
│                                                              │
│  for (int i = 0; i < 2000; i++) {  ← 各三角形/パーティクル │
│      triangle->SetPipelineCommands()  ← 何度も実行          │
│      triangle->SetWvpMatrix(wvp[i])                         │
│      triangle->SetColor(color[i])                           │
│      triangle->Draw(i)  ← DrawCall 発行                     │
│  }                                                           │
│                                                              │
│  ≈ 2000 個の DrawCall + 重複したパイプライン設定           │
└─────────────────────────────────────────────────────────────┘

【GPU コマンドキューに積まれるもの】
DrawCall #1:
  ├─ SetPipelineState
  ├─ SetGraphicsRootSignature
  ├─ SetDescriptorTable (テクスチャ)
  ├─ SetWVP (行列)
  └─ DrawInstanced(6 頂点, 1)

DrawCall #2:
  ├─ SetPipelineState        ← 同じなのに何度も設定！
  ├─ SetGraphicsRootSignature ← 同じなのに何度も設定！
  ├─ SetDescriptorTable      ← 同じなのに何度も設定！
  ├─ SetWVP (別の行列)
  └─ DrawInstanced(6 頂点, 1)

... × 2000 回
```

### 問題点
- **CPU-GPU 同期オーバーヘッド**: 毎 DrawCall に同じ PSO / RootSignature / テクスチャ設定を何度も送信
- **パイプラインフラッシュ**: PSO が同じでも、ドライバが検証・キャッシュ無効化を繰り返す
- **コマンドバッファサイズ肥大化**: 不要な重複コマンドで GPU メモリを浪費

### パフォーマンス結果
- **FPS: 20 FPS**（パーティクル 2000 個時）
- **ボトルネック**: GPU ドライバの描画準備処理

---

## スライド 2: アフター（最適化後）

```
【毎フレーム】
┌─────────────────────────────────────────────────────────────┐
│                     Game::Render()                          │
│                                                              │
│  // Step 1: 全三角形の行列・色を GPU バッファに書き込み      │
│  for (int i = 0; i < 2000; i++) {                           │
│      triangle->SetWvpMatrix(wvp[i], i);                     │
│      triangle->SetColor(color[i], i);                       │
│  }                                                           │
│                                                              │
│  // Step 2: Pipeline 設定を 1 回だけ実行                    │
│  triangle->SetPipelineCommands();  ← 1 回のみ！             │
│                                                              │
│  // Step 3: 各三角形を描画（1 DrawCall = 1 三角形）        │
│  for (int i = 0; i < 2000; i++) {                           │
│      triangle->Draw(i);  ← DrawCall 発行                    │
│  }                                                           │
│                                                              │
│  ≈ 2000 個の DrawCall（但し PSO 設定は 1 回のみ）          │
└─────────────────────────────────────────────────────────────┘

【GPU コマンドキューに積まれるもの】
SetPipelineState
SetGraphicsRootSignature
SetDescriptorTable (テクスチャ)

DrawCall #1:
  ├─ SetWVP (行列)
  └─ DrawInstanced(6 頂点, 1)

DrawCall #2:
  ├─ SetWVP (別の行列)
  └─ DrawInstanced(6 頂点, 1)

... × 2000 回

【重要】SetPipelineState 等は一度設定すると「状態がキープ」される
→ 次の DrawCall では改めて設定する必要なし
```

### 改善点
- **CPU-GPU 同期削減**: PSO / RootSignature / テクスチャ設定が 1 回で済む
- **パイプラインフラッシュ削減**: ドライバが 2000 回の「同じ設定」の検証をスキップ
- **コマンドバッファ効率化**: 不要な重複コマンド削減
- **D3D12 のステートマシン活用**: 一度設定した状態は保持される仕様を活用

### パフォーマンス結果（推定）
- **FPS: 40+ FPS**（理論値。実装は Task 21 予定）
- **ボトルネック**: GPU の実際の三角形ラスタライズ処理（今後、真の意味でのバッチ化で対応）

---

## 今後の最適化パス

```
現状 (Task 20):
┌──────────────────────────────────────┐
│  SetPipelineState (×1)               │  ← このタイミングで設定
│  SetRootSignature (×1)               │
│  SetDescriptorTable (×1)             │
│  ────────────────────────────────────│
│  DrawInstanced ← #1 (6 頂点, 1)      │
│  DrawInstanced ← #2 (6 頂点, 1)      │
│  DrawInstanced ← #3 (6 頂点, 1)      │
│  ...                                 │
│  DrawInstanced ← #2000 (6 頂点, 1)   │  ← 各 DrawCall は WVP だけ変更
└──────────────────────────────────────┘

将来（Task 21+ : 真のバッチ化）:
┌──────────────────────────────────────┐
│  SetPipelineState (×1)               │
│  SetRootSignature (×1)               │
│  SetDescriptorTable (×1)             │
│  ────────────────────────────────────│
│  DrawInstanced (全三角形, インスタンス数)  │  ← 1 DrawCall で全部
│                                      │
│  または                              │
│                                      │
│  IndexedIndirectDispatch で複数      │
│  DrawCall を GPU 側で発行             │
└──────────────────────────────────────┘
```

---

## まとめ

| 指標 | ビフォー | アフター |
|------|---------|---------|
| **SetPipelineState 呼び出し** | 2000 回 | 1 回 |
| **SetDescriptorTable 呼び出し** | 2000 回 | 1 回 |
| **コマンドバッファサイズ** | 大（肥大化） | 中（効率化） |
| **CPU-GPU 同期待機** | 長い | 短い |
| **FPS** | 20 FPS | 40+ FPS（推定） |

**学習ポイント**
- DirectX12 は「状態を一度設定すると保持する」ステートマシン
- GPU パイプラインの「準備」をいかに減らすかが鍵
- 本当の意味での「バッチ化」（1 DrawCall で複数オブジェクト）は次フェーズ
