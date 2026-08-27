#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// 横一列に並んだ薄いCubeの破線（例：名前入力欄の下線）を表示するコンポーネント。
// AlphabetTextComponentと同じ設計：このコンポーネント自身はGameObjectを生成する権限を
// 持たない（IComponentはシーンを知らない）ため、実際のダッシュ1本ごとの子GameObject
// （CubeRenderComponent付き）の生成・破棄はSceneBase::UpdateDashedLineComponents
// （毎フレーム、Render()から呼ばれる）が担当する。このコンポーネントは本数・間隔・太さ等の
// 「設定」と、SceneBase側が前回何を生成したかを検知するための内部状態だけを持つ
class DashedLineComponent : public IComponent {
public:
	// ダッシュ（1本の短い線分）の本数
	int dashCount = 8;

	// ダッシュ1本のX方向の長さ
	float dashWidth = 0.6f;

	// ダッシュの太さ（Y/Z方向のサイズ）
	float dashThickness = 0.07f;

	// ダッシュの中心から次のダッシュの中心までのX方向の距離。dashWidthより十分大きくしないと
	// ダッシュ同士が重なって実線に見える
	float dashSpacing = 1.8f;

	// 全ダッシュ共通の色（RGBA）
	Vector4 color = { 1.0f, 1.0f, 1.0f, 0.7f };

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// SceneBase::UpdateDashedLineComponentsが「前回子GameObjectを組み立てた時点の値」を
	// 控えておくために使う。dashCount/dashWidth/dashThickness/dashSpacingのいずれかが
	// 今の値と食い違っていたら子GameObjectを作り直す
	int lastBuiltDashCount = -1;
	float lastBuiltDashWidth = -1.0f;
	float lastBuiltDashThickness = -1.0f;
	float lastBuiltDashSpacing = -1.0f;
};
