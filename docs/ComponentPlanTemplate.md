# 新規コンポーネント計画書テンプレート

新しい`IComponent`派生クラスを作る前に、このテンプレートをコピーして空欄を埋める。
Claude（作業者）は、ユーザーが「新しいコンポーネントを作りたい」と言ったら、まずこのテンプレートを
提示して埋めてもらってから実装に着手する（[EngineGrowthPolicy.md](EngineGrowthPolicy.md)の
汎用化判断・[Components.md](Components.md)の既存一覧と突き合わせながら埋めるとよい）。

区分の意味や記入例は各項目末尾の「（例：...）」を参照。埋め終わったらそのまま貼り付けて渡せばよい。

---

## コンポーネント計画書

### 1. 名前・置き場所
- **クラス名**：（例：`GridWallDamageComponent`）
- **配置フォルダ**：`Engine/GameObject/Component/` の Render / Physics / Lighting / Audio のどれか、
  またはゲーム固有なら `Game/` 配下（どちらか迷う場合は[EngineGrowthPolicy.md](EngineGrowthPolicy.md)
  2節「エンジン層とゲーム層の境界」を参照。特定ゲームのタグ名・シーン遷移・ゲームロジックを
  知る必要があるなら`Game/`側）

### 2. 目的（1〜2文）
このコンポーネントが「何をするか」を一文で。既存コンポーネントに似たものが無いか
[Components.md](Components.md)を先に確認したか：あり/なし（あれば継承かコピー元にする）

### 3. 保持するデータ（メンバ変数）
| メンバ名 | 型 | 既定値 | 説明・単位 |
|---|---|---|---|
| | | | |

### 4. 振る舞い（Update/Draw等で毎フレーム何をするか）
- `Update(deltaTime, transform, ctx)`をoverrideするか：する/しない
  - するなら：何を読み、何を書き換えるか（例：「毎フレームtransform.translation.yを重力で書き換える」）
- `OnTriggerEnter(other)`をoverrideするか：する/しない
- 描画を持つか（`Draw()`独自メソッドを生やすか）：する/しない
  - するならシグネチャ（`RenderComponentBase`系に合わせるか、独自か）

### 5. 依存関係
- **コンストラクタ引数は要るか**：不要（デフォルト構築） / 要る（内容：　　）
  - 要る場合、[ComponentRegistry.h](../Engine/GameObject/ComponentRegistry.h)の
    `RegisterSimple`は使えないため、`Register<T>`をComponentRegistration.cppへ個別登録する必要がある
- **兄弟コンポーネントに依存するか**（例：MirrorComponentが兄弟CubeRenderComponentへの
  ポインタを持つパターン）：しない / する（対象：　　）
  - 依存する場合、削除順序のガードが要るか（先に依存先を消してもらう必要があるか）
- **外部リソース（モデル・テクスチャ・音声等）を読むか**：しない / する（内容：　　）

### 6. 保存（ToJson/FromJson）
- 保存対象にするか：する/しない（一時状態のみ・毎回既定値から始めてよいものはしない。
  GravityComponent::velocityYのように「実行中の一時状態」は保存しない判断も可）
- 保存するメンバ一覧（3節の表からJSONへ出す項目だけ抜粋）：
- 保存しないメンバとその理由：

### 7. Inspector（DrawImGui）
- 表示する項目：3節の表のうちUIから調整したいもの
- 特殊なUI（コンボ・ボタン等）が要るか：不要 / 要る（内容：　　）

### 8. ComponentRegistry登録方法
- `RegisterSimple<T>`で足りるか（デフォルト構築・他コンポーネント非依存）：はい/いいえ
  - はいなら：`.cpp`末尾に`REGISTER_SIMPLE_COMPONENT(Type, "typeName", "表示名", "カテゴリ")`を1行追加するだけ
  - いいえなら：`ComponentRegistration.cpp`の`RegisterEngineComponents()`へ`Register<T>`を追加
    （5節の依存関係を踏まえたcreator/removerラムダが必要）

### 9. 汎用化チェック（EngineGrowthPolicy.md 1節）
- 同じ形のコードを書くのはこれで何回目か（エンジン全体通算）：1回目 / 2回目 / 3回目以上
  → 3回目以上なら関数/クラスへの切り出しを検討
- 既存コンポーネントの継承・コピー元にできるものはあったか：あり（　　） / なし

### 10. 完成の確認方法
- どう動けば完成とみなせるか（例：「盤面に配置してPlayすると壁マスを踏んだ瞬間に攻撃力が-1される」）
- Editor（Sceneビュー）で見た目を確認する必要があるか：はい/いいえ

---

## 実装チェックリスト（Claude側の実施手順）

1. `Engine/GameObject/Component/<カテゴリ>/<ClassName>.h`を作成（`IComponent`継承）
2. 3〜7節の内容を反映（メンバ変数・Update・ToJson/FromJson・DrawImGui）
3. `.cpp`を作成し、実装本体を書く
4. 8節に従って登録：
   - Simple：`.cpp`末尾に`REGISTER_SIMPLE_COMPONENT(...)`
   - カスタム：`ComponentRegistration.cpp`の`RegisterEngineComponents()`に追記
5. 該当カテゴリの便利ヘッダ（`Render.h`/`Physics.h`/`Lighting.h`/`Audio.h`）に`#include`を追加
6. `.vcxproj`/`.vcxproj.filters`にファイルを追加（新規ファイル作成時は必須）
7. ビルドが通ることを確認
8. 10節の完成条件をSceneビュー/Playで実際に確認する
9. [Components.md](Components.md)に該当コンポーネントの説明を追記するか判断する
   （エンジン全体で再利用され得るものは追記、特定ゲーム限定のものは省略可）
