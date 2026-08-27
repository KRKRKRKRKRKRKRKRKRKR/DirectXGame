#pragma once
#include "RenderComponentBase.h"

// ReflexPlayerComponent::DrawPlanningVisualizationが計画フェーズの経路マーカーとして表示する
// Circle.objの波紋パルスアニメーションと同じ見た目を、GameObjectに常時表示するためのコンポーネント。
// チュートリアルで「ここをクリックできる」ことを示す目印（TutorialScene::HandleSceneTransitionInputが
// 敵4体の初期位置に配置する）として使う想定。ReflexPlayerComponent側のロジックは
// waypoints_（プレイヤーの経路予約）に紐付いていて再利用できないため、同じパラメータ・
// 同じ計算式をここに複製している
class ClickHintMarkerComponent : public RenderComponentBase {
public:
	// 波紋の最小/最大スケールと1周期の長さ（秒）。ReflexPlayerComponent::markerPulseMinScale/
	// markerPulseMaxScale/markerPulseDurationと同じ意味
	float pulseMinScale = 0.15f;
	float pulseMaxScale = 0.3f;
	float pulseDuration = 1.0f;

	// 同時発生させる波紋の本数。ReflexPlayerComponent::markerWaveCountと同じ意味
	int waveCount = 3;

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

private:
	// Circle.objの遅延ロード状態。ReflexPlayerComponentと同じ理由（コンストラクタがRendererを
	// 受け取れないため、初回Draw時に1度だけLoadModelする）
	mutable bool tryLoadCircleModel_ = false;
	mutable bool circleModelLoaded_ = false;
	mutable Renderer::ModelHandle circleModelHandle_ = 0;

	// 波紋アニメーションの基準経過時間。Updateでだけ加算し、Drawは読むだけ
	// （GridBackgroundComponentと同じ理由：DrawはRenderMirrorPass等からdeltaTime=0固定で
	// 呼ばれることがあるため、Draw自身で時間を進めると鏡越しの見た目が変わってしまう）
	float elapsed_ = 0.0f;
};
