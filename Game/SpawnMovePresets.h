#pragma once
#include "../Engine/GameObject/Component/Physics/SpawnMoveComponent.h"
#include "../Math/MathTypes.h"

// SpawnMoveComponentを使った「現在位置から奥へ縮小しながら消える」退場演出は、
// ClearScene::PlayBackspaceExitAnimation（文字削除）とTutorialScene::StartHintExitAnimation
// （操作説明文言）で、duration/zOffset/easingが全く同じ値のまま別ファイルに複製されていた
// （TutorialScene側のコメントで「ClearScene::PlayBackspaceExitAnimationのkExitZOffset/
// kExitDurationと同じ値」と明記されていたコピペ値）。この値だけを1箇所にまとめる。
// SpawnMoveComponent自体の汎用化（回転対応・コールバック等）は行わない
namespace SpawnMovePresets {
	// AlphabetTextComponent::entranceZOffset/entranceDuration（既定6.0f/0.35f）と揃えた値
	constexpr float kExitZOffset = 6.0f;
	constexpr float kExitDuration = 0.35f;
	constexpr Easing::Type kExitEasing = Easing::Type::kInCubic;

	// targetの現在のワールド座標(worldPos)を起点に、Z+方向へkExitZOffsetぶん移動しながら
	// 現在のscaleから0へ縮小する退場演出を開始する。呼び出し側は先にRemoveComponent
	// <SpawnMoveComponent>()、後でRebuildDerivedLists()を呼ぶこと（このヘルパーは
	// AddComponent以降のパラメータ設定のみ担当する）
	inline void ApplyExit(SpawnMoveComponent& spawnMove, const Vector3& worldPos, const Vector3& currentScale) {
		spawnMove.startPos = worldPos;
		spawnMove.targetPos = worldPos + Vector3{ 0.0f, 0.0f, kExitZOffset };
		spawnMove.duration = kExitDuration;
		spawnMove.easing = kExitEasing;
		spawnMove.elapsed = 0.0f;
		spawnMove.finished = false;
		spawnMove.animateScale = true;
		spawnMove.targetScale = currentScale; // 消える直前の見た目サイズを起点にする
		spawnMove.reverseScale = true; // targetScale→0（縮小しながら消える）
		spawnMove.destroyOnFinish = true;
	}
}
