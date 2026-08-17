#pragma once
#include "../../IComponent.h"

// このGameObjectが「パーティクルテンプレート」であることを示す目印コンポーネント。
// ReflexEnemyComponent::isTemplateと同じ設計方針：実際の見た目（CubeRenderComponent等）・
// 発生設定（ParticleEmitterComponent）は今までどおりこのGameObjectに個別に付けたまま使う
// （重複を避けるため、このコンポーネント自身は値を持たない）。
//
// PlayScene::OnInitializeがシーン内を走査してこのコンポーネントを持つGameObjectを全て見つけ、
// Play開始時に一括で隠す（フィールド外へ退避させる。パーティクル自体はTrigger判定を持たないため
// 当たり判定を外す必要はない）。PlayScene::SpawnParticleBurstAtが該当タグのGameObjectから
// ParticleEmitterComponent・見た目（色・形状）を読み取り、指定位置に粒子一式を複製生成する
class ParticleTemplateComponent : public IComponent {
public:
	void DrawImGui(const char* namePrefix) override;
};
