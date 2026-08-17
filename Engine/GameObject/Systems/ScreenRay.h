#pragma once
#include "../../../Math/MathTypes.h"
#include "../../../Math/Collision.h"

class Renderer;

// マウスのスクリーン座標からワールド空間のレイを生成する。GizmoController::UpdatePickingの
// ピッキング用レイキャストと、ゲームロジック側のクリック判定（地面Planeとの交点取得等）の
// 両方から共有して使う（重複実装を避けるための抽出）
namespace ScreenRay {

// 現在のマウス位置（ImGui::GetMousePos()基準）から、Sceneビューの矩形オフセットを
// 差し引いた上でNDC変換し、view*projの逆行列でワールド空間のレイに変換する
Collision::Ray FromMouse(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj);

} // namespace ScreenRay
