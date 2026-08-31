# ClearRootEngine 育成方針

このエンジンは今後、REFLEX以外の複数のゲームを開発しながら少しずつ育てていく前提で作られている。
「ゲームを完成させる」ことと「エンジンの汎用性を上げる」ことはしばしばトレードオフになるため、
判断に迷ったときにこのドキュメントを基準にする。

> 作成日: 2026-08-29。ゲームごとの個別事情はこのファイルではなく各ゲームのドキュメント
> （REFLEXなら[ドキュメント.md](../ドキュメント.md)等）に書く。ここに書くのは
> ゲームを跨いで通用する判断基準・バックログのみ。

## 1. 汎用化するかどうかの判断基準

1. **同じ形のコードを3回書きそうになったら、その時点で関数/クラスへ切り出す**。
   2回目までは複製で進めてよい（早すぎる抽象化を避ける）。ただし「今のゲームで2回」
   ではなく「エンジン全体を通算して」数える。1つ目のゲーム（REFLEX）で2回、
   次のゲームで似た処理が1回出てきたら、それが3回目＝切り出しどき。
2. シグネチャ・戻り値の意味が完全一致する関数（例：`ColliderComponentBase::
   GetGizmoEditTransform`）は即座に基底へ引き上げる。戻り値の型や計算内容が
   本質的に異なるもの（例：`GetWorldSphere`/`GetWorldOBB`）は無理に統一しない。
3. 新規コンポーネントを作る前に、既存コンポーネント一覧（Inspector「+コンポーネントを
   追加」メニュー、または[docs/Components.md](Components.md)）に似た形のものが
   無いか探し、あれば継承かコピーの出発点にする。
4. モデルの遅延ロードが必要なコンポーネントは`Engine/Graphics/Renderer/
   LazyModelHandle.h`をメンバに1個持つ（`tryLoad_`/`loaded_`/`handle_`の3変数を
   手書きしない）。
5. 演出パラメータ（duration/easing/オフセット等）を使う前に、既存コンポーネントに
   同種の値が無いか確認する。値を変える場合は「なぜ違うのか」をコメントで明示する
   （例：`Game/SpawnMovePresets.h`）。
6. クリック判定ボタン（見た目+当たり判定+ホバー反映+クリック検知）を増やす場合は
   `SceneBase::UpdateButtonAndReflectHover`を使う。
7. ToJson/FromJson/DrawImGuiは3点セットで必ず同時に更新する。

## 2. 「エンジン層」と「ゲーム層」の境界

- `Engine/`配下のコンポーネント・システムは、特定のゲーム（REFLEX等）のタグ名・
  シーン遷移先・ゲームロジックを知ってはいけない。実例：`PlayButtonComponent`
  （`Engine/GameObject/Component/Physics/PlayButtonComponent.h`）はクリック検知・
  ホバー判定・SE再生のみを持ち、シーン遷移は`Game/`側のシーンクラスに委譲している。
- 逆に、特定のゲームだけに関わるロジック（例：REFLEXの`ReflexEnemyComponent`の
  HP/hitShake、`EnemySpawnManager`のテンプレート複製）は`Engine/`に汎用化を
  急がず、まず`Game/`側に置いたままでよい。次のゲームで同種の需要が出てから
  `Engine/`側へ引き上げるかどうかを判断する（1節の「3回目」基準に従う）。

## 3. 実装保留中のバックログ（次のゲームで需要が出たら着手）

### `SceneBase::CloneObject` — GameObject汎用複製API

REFLEXの`EnemySpawnManager`（`Game/EnemySpawnManager.cpp`）が持つ「テンプレート
GameObjectからコンポーネントを1つずつ手作業で読み取り、複製先に付け直す」処理は、
既存の`GameObject::ToJson`/`FromJson`（`Engine/GameObject/GameObject.cpp:83-151`）
のラウンドトリップで汎用化できる設計が固まっている：

```cpp
// SceneBase.h（protected、CreateObjectの直後）
GameObject& CloneObject(GameObject& source, const std::string& newName);

// SceneBase.cpp
GameObject& SceneBase::CloneObject(GameObject& source, const std::string& newName) {
    nlohmann::json data;
    source.ToJson(data);
    GameObject& clone = CreateObject(newName);
    ComponentLoadContext ctx = MakeComponentLoadContext();
    clone.FromJson(data, ctx);
    clone.name = newName;
    RebuildDerivedLists();
    return clone;
}
```

既知の制約：
- `pickingRadiusHint`・`excludeFromSave`・親子関係はToJson/FromJson対象外のため
  複製されない
- 兄弟コンポーネント依存型（`ReflexEnemyHealthBarComponent`等）は`GameObject::
  FromJson`の`kDeferredTypes`（現状`TextureSelector`/`Mirror`のみ）に入っていないと
  復元順序で失敗しうる
- 複製は「まるごとコピー」に留め、個体差（ランダムサイズ・HP初期化等）は
  呼び出し側が複製後に上書きする

**着手条件**: 次のゲームで「テンプレートから量産する」需要（敵、アイテムドロップ、
NPC、弾等）が発生したタイミング。最初の置き換え対象はREFLEXの
`SpawnParticleBurstAt`を推奨（兄弟依存が無く、壊れてもゲームプレイに直結しない）。

## 4. ゲームをまたいで残す/残さないものの目安

- **エンジンに残す**: `Engine/GameObject/`のコンポーネント基盤（IComponent、
  ComponentRegistry）、`Engine/Graphics/`の描画パイプライン、`Engine/GameObject/
  Systems/`の当たり判定・Gizmo等の汎用システム、`SceneBase`のエディタ機能一式
  （Hierarchy/Inspector/保存読み込み/Scene・Gameビュー切替）。
- **ゲームごとに作り直す/破棄してよい**: `Game/`配下のシーンクラス自体
  （TitleScene, PlayScene等）とゲーム固有コンポーネント（Reflex*系）。次のゲームで
  流用したくなった処理があれば、その時点で1節の基準に従ってEngine層へ汎用化する。
