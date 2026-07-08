#pragma once
#include "../../../../Math/MathTypes.h"

class Renderer;

// Directional/Point/SpotLightComponentが共通して持つ「SceneLightへの反映」「Gizmo可視化」の
// 2メソッドだけを束ねる純粋インターフェース。3つのLightComponentはデータ（color等）をほとんど
// 共有しないため実装の共通化はしない（早すぎる抽象化を避ける）が、PlayScene::Render()が
// 具体型を気にせず汎用ループで全ライトを処理できるよう、ディスパッチ専用にこれだけ用意する
class ILightComponent {
public:
	virtual ~ILightComponent() = default;
	virtual void SyncToRenderer(Renderer* renderer, const Transform& transform) const = 0;
	virtual void DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const = 0;
};
