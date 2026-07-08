# 使用可能なコンポーネント一覧

`GameObject`にアタッチできる`IComponent`派生クラスの一覧。`Engine/GameObject/Component/`配下にまとまっている。

## 共通の仕組み

- `IComponent`（`Engine/GameObject/IComponent.h`）が全コンポーネントの最小抽象。`virtual void Update(float deltaTime, Transform& transform)`と`virtual void DrawImGui(const char* namePrefix)`の2つを持ち、どちらもデフォルトは何もしない。`Update`がオーナーの`Transform&`を直接受け取れるため、`GravityComponent`のように毎フレームTransformを書き換えるコンポーネントも追加パラメータなしで実装できる。
- `GameObject`（`Engine/GameObject/GameObject.h`）は`AddComponent<T>(...)`でコンポーネントを追加し、`GetComponent<T>()`（`dynamic_cast`で検索）で取り出す。`Update()`/`DrawImGui()`は保持している全コンポーネントに対して型を気にせず一括で呼ぶだけ（`for (auto& c : components_) c->Update(deltaTime);`のような形）。
- **`TransformComponent`は全GameObjectが生成時に自動で1つ持つ**（`GameObject`のコンストラクタで`AddComponent<TransformComponent>()`済み）。他のコンポーネントは`AddComponent<T>()`で明示的に追加する必要がある。
- `DrawImGui(namePrefix)`をoverrideしているコンポーネントは、`GameObject::DrawImGui()`を呼ぶだけで自動的に自分の項目（見出し無しの生の値のみ）がImGuiに表示される。ラベルは`"{namePrefix} 項目名"`という形式になる。

```cpp
GameObject obj;
obj.name = "Sample";
CubeRenderComponent* render = obj.AddComponent<CubeRenderComponent>();
render->textureHandle = someHandle;
OBBColliderComponent* collider = obj.AddComponent<OBBColliderComponent>();
collider->layer = CollisionLayer::kObstacle;

obj.GetTransform().translation = { 0.0f, 1.0f, 0.0f };
obj.DrawImGui(); // Transform/Render/Colliderの項目がまとめて描画される
```

---

## TransformComponent

`Engine/GameObject/Component/TransformComponent.h`

全GameObjectが必ず1つ持つ、位置・回転・スケールのコンポーネント。`GameObject::GetTransform()`経由でアクセスする。

| メンバ | 型 | 説明 |
|---|---|---|
| `transform` | `Transform`（scale/rotation/translation） | 実データ本体 |
| `scaleSpeed`, `scaleMin`, `scaleMax` | `float` | ImGuiドラッグの刻み幅・範囲（Scale用） |
| `rotationSpeed` | `float` | 回転ドラッグの刻み幅（範囲は常に±π固定） |
| `translationSpeed`, `translationMin`, `translationMax` | `float` | ImGuiドラッグの刻み幅・範囲（Translation用） |
| `is2D` | `bool` | trueならSprite2D向けの2D表示（Pos(px)/Size(px)+Z軸回転のみ、`DragFloat2`使用）に切り替わる |

オブジェクトごとに見た目のスケール感が違う場合は、`obj.GetComponent<TransformComponent>()`で取得して`scaleMin`等を個別に上書きする。

---

## RenderComponentBase 系（描画）

`RenderComponentBase`（`Engine/GameObject/Component/RenderComponentBase.h`）が描画コンポーネント共通の基底クラス。

### 共通プロパティ（RenderComponentBase）

| メンバ | 型 | 説明 |
|---|---|---|
| `color` | `Vector4` | 色 |
| `lighting` | `bool` | ライティング有効/無効 |
| `textureHandle` | `TextureHandle` | 使用するテクスチャ |
| `blendMode` | `BlendMode` | None/Normal/Add/Subtract/Multiply/Screen |
| `blendStrength` | `float` | ブレンドの強さ（0〜1） |
| `alphaTest` | `bool` | αテスト（2値抜き）の有効/無効 |
| `alphaThreshold` | `float` | αテストのしきい値 |

`DrawImGui(namePrefix)`で上記全部（Lighting/Color/BlendMode/Blend Strength/Alpha Test）をまとめて描画する。

### 派生クラス

| クラス | 用途 | 固有メンバ・注意点 |
|---|---|---|
| `CubeRenderComponent` | 立方体（Floor/Mirror等、板状に引き伸ばして流用することも多い） | 固有メンバなし |
| `SphereRenderComponent` | 球 | 固有メンバなし。Subdivision（分割数）はRenderer側のグローバル設定のためコンポーネントには持たせない |
| `TriangleRenderComponent` | 三角形 | 固有メンバなし。Smoothnessも同様にグローバル設定 |
| `ModelRenderComponent` | OBJ/FBXモデル（ボーンアニメーション対応） | コンストラクタで`Renderer::ModelHandle`と`hasAnimation`を受け取る。`Draw()`が`deltaTime`も取る点が他と異なる（`hasAnimation`がtrueならアニメーション更新を行う） |
| `SpriteRenderComponent` | 3D/2Dスプライト（ワールド空間・スクリーン空間の両対応） | コンストラクタで`is3D`を受け取る。`uvTransform`（UVのOffset/Rotation/Scale）を追加で持ち、`DrawImGui`をoverrideしてUV Transformの項目も追加描画する |

---

## TextureSelectorComponent（テクスチャ選択）

`Engine/GameObject/Component/TextureSelectorComponent.h`

`RenderComponentBase`系コンポーネントに「テクスチャ選択コンボ」を後付けするコンポーネント。単体では意味を持たず、必ず対象の`RenderComponentBase*`とセットで使う。

```cpp
CubeRenderComponent* render = obj.AddComponent<CubeRenderComponent>();
render->textureHandle = textures_[1].handle; // 初期テクスチャ
obj.AddComponent<TextureSelectorComponent>(render, &textures_, 1);
```

`DrawImGui(namePrefix)`が"{namePrefix} Texture"というコンボを描画し、選択結果を毎回`target->textureHandle`へ書き戻す（`PlayScene::textures_`のような共有テクスチャ一覧への`const`ポインタを持つのみで、一覧自体は所有しない）。

---

## ColliderComponentBase 系（当たり判定）

`ColliderComponentBase`（`Engine/GameObject/Component/ColliderComponentBase.h`）が当たり判定コンポーネント共通の基底クラス。

### 共通プロパティ（ColliderComponentBase）

| メンバ | 型 | 説明 |
|---|---|---|
| `layer` | `CollisionLayer` | 自分の所属レイヤー |
| `isTrigger` | `bool` | true=Trigger（検知のみ、押し戻さない）／false=Solid（重なったら押し戻す） |
| `collidesWith[kCount]` | `bool[]` | 衝突判定の対象として選んでいるレイヤー一覧（デフォルト全true） |

`ShouldLayersCollide(a, b)`：お互いが相手の`layer`を自分の`collidesWith`に含めている場合のみtrue（片方だけでは衝突しない、両者合意方式）。`DrawImGui(namePrefix)`でLayerコンボ・Is Triggerチェックボックス・Collides Withのチェックボックス一覧を描画する。

### CollisionLayer（`CollisionLayer.h`）

```cpp
enum class CollisionLayer { kDefault, kPlayer, kObstacle, kItem, kEnvironment, kCount };
```

### 派生クラス

| クラス | 用途 | 固有メンバ |
|---|---|---|
| `SphereColliderComponent` | 球形の当たり判定 | `offset`（オーナー位置からの相対位置）、`radius`（ワールド単位、scaleの影響を受けない絶対値） |
| `OBBColliderComponent` | 回転追従の直方体（OBB）当たり判定 | `offset`、`halfSize`（オーナーの`transform.scale`が自動で掛かる）。回転はオーナーのTransform.rotationをそのまま使う |

どちらも`GetWorldSphere(ownerTransform)`/`GetWorldOBB(ownerTransform)`でワールド座標の形状を取得し、`Collision::`名前空間の関数（`SphereSphere`/`OBBOBB`/`OBBSphere`とその`*Penetration`版）で判定・押し戻し計算を行う（`PlayScene::ResolveAndDrawColliderGizmos`参照）。`DrawWireframe(...)`でワイヤーフレーム描画もできる。

---

## GravityComponent（重力）

`Engine/GameObject/Component/GravityComponent.h`

`Update(deltaTime, transform)`で速度を積算し`transform.translation.y`を動かすだけの単純な重力。地面判定自体は持たない。

| メンバ | 型 | 説明 |
|---|---|---|
| `enabled` | `bool` | falseなら更新をスキップ |
| `gravity` | `float` | 重力加速度 |
| `velocityY` | `float` | 現在の落下速度（内部状態。着地時の押し戻しでリセットされる） |
| `maxFallSpeed` | `float` | 終端速度。無制限に加速すると1フレームの移動量が薄い床の厚みを超えてすり抜ける原因になるため上限を設けている |

地面で実際に止まるには、このGameObjectが以下をすべて満たす必要がある（着地は`GravityComponent`単体ではなく`PlayScene::ResolveAndDrawColliderGizmos`の押し戻し処理との連動で実現している）。

1. Collider（Sphere/OBB）を持つ
2. Solid（`isTrigger = false`）
3. 地面側のレイヤーと`collidesWith`が噛み合っている
4. `gizmoTargets_`に登録されている

---

## ライトコンポーネント（DirectionalLight / PointLight / SpotLight）

`Engine/GameObject/Component/DirectionalLightComponent.h` / `PointLightComponent.h` / `SpotLightComponent.h`

Unityのライトと同じく、**「空のGameObject」にライトコンポーネントを1つ付ける**という使い方をする（`Render`/`Collider`コンポーネントは持たせない）。位置・向きは自分では持たず、オーナーの`TransformComponent`（`translation`/`rotation`）から導出するため、他のオブジェクトと全く同じくGizmoで動かす・回転させるだけで光も連動する。共通の基底クラスは作っていない（共有できるのは`enabled`/`color`程度で、`SyncToRenderer`の中身が3つとも全く違うため、共通化の効果が薄い）。

- `SyncToRenderer(Renderer*, const Transform&)`：`Renderer::GetLight()`のSetter経由で`SceneLight`（GPU cbuffer）へ反映する。`IComponent::Update`には乗せていないため、**毎フレーム明示的に呼ぶ必要がある**（`PlayScene::Render()`内で1回呼んでいる）。
- `DrawGizmoVisualization(Renderer*, const Transform&, view, proj)`：デバッグ用の球・ラインを描画する（`ColliderComponentBase::DrawWireframe`と同じ考え方）。

```cpp
GameObject pointLightObject;
pointLightObject.name = "Point Light";
pointLightObject.GetTransform().translation = { 0.0f, 2.0f, 0.0f };
PointLightComponent* light = pointLightObject.AddComponent<PointLightComponent>();
light->enabled = true;

// 毎フレーム（Render側で）
light->SyncToRenderer(renderer_, pointLightObject.GetTransform());
light->DrawGizmoVisualization(renderer_, pointLightObject.GetTransform(), view, proj);
```

### DirectionalLightComponent

| メンバ | 型 | 説明 |
|---|---|---|
| `enabled` | `bool` | 有効/無効 |
| `color` | `Vector3` | 光の色 |
| `ambient` | `float` | 環境光の強さ |
| `halfLambertPower` | `float` | ハーフランバートの係数 |

向きは`transform.rotation`を`TransformMath::EulerRadiansToDirection`で方向ベクトルに変換して使う（Directionを直接編集するフィールドは持たない）。

### PointLightComponent

| メンバ | 型 | 説明 |
|---|---|---|
| `enabled` | `bool` | 有効/無効 |
| `color` | `Vector3` | 光の色 |
| `intensity` | `float` | 強さ |
| `radius` | `float` | 減衰半径 |
| `decay` | `float` | 減衰の急さ |

位置は`transform.translation`をそのまま使う。回転・スケールの概念はない。

### SpotLightComponent

| メンバ | 型 | 説明 |
|---|---|---|
| `enabled` | `bool` | 有効/無効 |
| `color` | `Vector3` | 光の色 |
| `intensity` | `float` | 強さ |
| `distance` | `float` | 届く距離 |
| `decay` | `float` | 減衰の急さ |
| `cosAngle` | `float` | 外側コーンのcos（この角度より外は完全に暗い） |
| `cosFalloffStart` | `float` | 内側コーンのcos（この角度より内は完全に明るい） |

位置は`transform.translation`、向きは`transform.rotation`から導出する（DirectionalLightと同じ変換）。

---

## RenderComponentFactory（ユーティリティ）

`Engine/GameObject/Component/RenderComponentFactory.h`

データ駆動生成（実行時にenum値から具体的なコンポーネントを生成する）を見据えたヘルパー。

```cpp
enum class RenderType { kCube, kSphere, kTriangle, kSprite, kModel };
struct RenderComponentDesc {
    bool is3D = true;                    // kSpriteのみ使用
    Renderer::ModelHandle modelHandle = 0; // kModelのみ使用
    bool hasAnimation = false;             // kModelのみ使用
};
RenderComponentBase* CreateRenderComponent(GameObject& obj, RenderType type, const RenderComponentDesc& desc = {});
```

`Draw()`はコンポーネントごとにシグネチャが異なる（`ModelRenderComponent`だけ`deltaTime`を取る等）ため`IComponent`の仮想関数にしておらず、`Draw()`を呼ぶ際は結局`GetComponent<具体型>()`で取り出す必要がある点に注意。

---

## 現状の制約

- 1つのGameObjectに同じ型のコンポーネントを複数付けても`GetComponent<T>()`は最初の1個しか返せない（`dynamic_cast`による検索のため）。実質「1オブジェクト1描画コンポーネント・1コライダー」運用。
- `Draw()`はIComponentの仮想関数ではないため、型を気にせず呼びたい場合（`Update`/`DrawImGui`のように）は使えず、`GetComponent<具体型>()`経由で呼ぶ必要がある。
