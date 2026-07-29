#pragma once
#include "../../../../Math/MathTypes.h"

class Renderer;

// Point/SpotLightは複数個シーンに置けるが、SceneLightの配列は種類ごとに固定長のため、
// 「今フレーム何番目に見つかったこの種類のライトか」をSceneBase::Render側で種類ごとに
// 数える必要がある。ダウンキャストせずに種類を判定するための最小限のタグ
enum class LightType { kDirectional, kPoint, kSpot };

// Directional/Point/SpotLightComponentが共通して持つ「SceneLightへの反映」「Gizmo可視化」の
// 2メソッドだけを束ねる純粋インターフェース。3つのLightComponentはデータ（color等）をほとんど
// 共有しないため実装の共通化はしない（早すぎる抽象化を避ける）が、PlayScene::Render()が
// 具体型を気にせず汎用ループで全ライトを処理できるよう、ディスパッチ専用にこれだけ用意する
class ILightComponent {
public:
	virtual ~ILightComponent() = default;
	virtual LightType GetLightType() const = 0;
	// slotIndex: このコンポーネントと同じGetLightType()を持つコンポーネントの中で、今フレーム
	// 何番目に処理されたか（SceneBase::Renderが種類ごとに数えて渡す）。DirectionalLightは
	// 単一枠のため無視してよい。Point/Spotは各々SceneLight::LightData::pointLights/spotLights
	// 配列のインデックスとして使う（配列の上限を超えた分は無視される）
	virtual void SyncToRenderer(Renderer* renderer, const Transform& transform, uint32_t slotIndex) const = 0;
	virtual void DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const = 0;
};
