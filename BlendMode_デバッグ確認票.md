# BlendMode デバッグ確認票

- 症状が出ているモード：GridCubeのMultiply
- blendStrengthの値：
- 期待する見た目：すべてのオブジェクトが透過してほしい
- 実際の見た目：順序がcamera->GridCube->Triangle,Sprite3Dは見える　他のオブジェクトは透過せず見えない｡　GridCube以外のモードはNone
- 描画順序（他オブジェクトとの前後関係）：

## 有力な原因（要検証）

GridCube(kMultiply)は色は薄く合成されるが、**深度バッファには通常通り書き込まれる**
（Pipeline.cpp DepthStencilState: DepthWriteMaskはBlendModeに関係なく常にALL）。
そのため、カメラとSphere/Model/Sprite2Dの間にGridCubeが入り込むと、
そのGridCubeのピクセルが先に深度を埋めてしまい、奥のオブジェクトが深度テストで
棄却されて描画されない（色が薄くても「そこにある」扱いになる）。
Triangleだけ見えているのは、たまたまカメラとの位置関係でGridCubeに遮られなかった可能性が高い。

## 切り分けのための追加確認

- [ ] カメラの位置・向き（現在地はどこから見ているか）
- [ ] Sphere/Model/Sprite2Dの座標とカメラの位置関係（GridCubeの後ろに隠れていないか、動かすと見えるか）
- [ ] GridCubeを一時的に非表示にした場合、Sphere/Model/Sprite2Dは見えるか
- [ ] GridCubeのY座標・スケールとSphere/Model/Sprite2DのY座標を比較（同じ高さで重なっていないか）

## 注意（仕様）

- kMultiply / kScreen は PS側でSrcColorを白(Multiply)/黒(Screen)へ補間して強さを反映（b2経由）
- kNormal / kAdd / kSubtract の blendStrength はピクセルのアルファではなく、BlendFactorという描画コマンドごとの定数

## 解決済：白テクスチャ×Multiplyが真っ黒になる件

原因：画面クリアカラーが黒 (0,0,0,1)（SwapChainManager.cpp:95）。
kMultiplyは 最終色 = Dest * SrcColor。背景（Dest）が黒の場所に描くと、
SrcColorが白でも 黒×白=黒 になる。これはPhotoshopの乗算レイヤーと同じ
正しい仕様で、バグではない。
kNoneに戻すと白く見えるのは、kNoneが上書き（Destを無視）のため。

Multiplyの効果を確認したい場合は、Multiplyのオブジェクトより先に
明るい下地（kNoneの背景やテクスチャ）を同じ場所に描画してから重ねる必要がある。
