#pragma once
#include "Easing.h"
#include "../Externals/imgui/imgui.h"
#include <cmath>

// イージングコンボの項目にカーソルを合わせたときのプレビュー描画。
// 元々ReflexPlayerComponent.cppのローカル関数だったが、ParticleEmitterComponent・
// SpawnMoveComponent等、他のイージング選択コンボからも同じプレビューを使いたくなったため、
// Math/Easing.hと対になる共有ヘッダへ切り出した
namespace EasingPreview {

	// 一定周期（kPreviewPeriodSeconds秒）で0→1に進む時間を作り、そのtにtypeを適用した
	// 位置に球体を描く。左端が開始、右端が終点で、往復はせず到達したら先頭に戻る。
	// 呼び出し側はImGui::BeginTooltip()/EndTooltip()で挟んで使う想定
	inline void Draw(Easing::Type type) {
		constexpr float kPreviewPeriodSeconds = 1.2f;
		constexpr float kPreviewWidth = 160.0f;
		constexpr float kPreviewHeight = 32.0f;

		float t = fmodf(static_cast<float>(ImGui::GetTime()), kPreviewPeriodSeconds) / kPreviewPeriodSeconds;
		float easedT = Easing::Apply(type, t);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 origin = ImGui::GetCursorScreenPos();
		ImVec2 trackStart = { origin.x, origin.y + kPreviewHeight * 0.5f };
		ImVec2 trackEnd = { origin.x + kPreviewWidth, origin.y + kPreviewHeight * 0.5f };

		drawList->AddLine(trackStart, trackEnd, IM_COL32(120, 120, 120, 255), 2.0f);

		float ballX = trackStart.x + (trackEnd.x - trackStart.x) * easedT;
		float ballRadius = kPreviewHeight * 0.25f;
		drawList->AddCircleFilled({ ballX, trackStart.y }, ballRadius, IM_COL32(255, 230, 60, 255));

		ImGui::Dummy({ kPreviewWidth, kPreviewHeight });
	}

	// コンボのSelectable直後に呼ぶ定型パターン（ホバー中ならツールチップでプレビューを出す）。
	// 各DrawImGuiでの呼び出しを1行に揃えるためのヘルパー
	inline void ShowOnHover(Easing::Type type) {
		if (ImGui::IsItemHovered()) {
			ImGui::BeginTooltip();
			Draw(type);
			ImGui::EndTooltip();
		}
	}

}
