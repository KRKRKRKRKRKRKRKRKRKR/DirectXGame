#pragma once
#include "../../IComponent.h"
#include "../../../../Math/Easing.h"
#include <string>

// プレイヤーGameObjectに付けて使う。コンボが発生するたびにRequestPopup(comboValue)を呼ぶと、
// 頭上に数字（Resources/Alphabet/{数字}.objの3Dモデル、AlphabetTextComponentと同じ読み込み元）が
// 表示される。キルカウントHUDと同じ「1つの表示が値の更新に合わせて差し替わる」方式：
// 新しいコンボ値が来るたびに、表示中の数字があれば即座に消して新しい数字でポップインをやり直す。
// holdDuration以内に次のコンボが来なければ自動的にフェードアウトして消える
// （実行フェーズが終わって手が止まればコンボ表示も自然に消える）。
//
// 実際のポップアップ用子GameObject（ModelRenderComponent付き）の生成・毎フレームのscale/alpha
// 更新・寿命が尽きた際の破棄は、このコンポーネント自身ではなくSceneBase::UpdateComboPopupComponents
// （IComponentはシーンを知らないため）が行う。このコンポーネントはリクエストと演出パラメータ・
// 現在表示中のポップアップの状態だけを持つ
class ComboPopupComponent : public IComponent {
public:
	// 1つのポップアップの進行状態。SceneBase側が生成・更新・破棄を管理する
	struct ActivePopup {
		int comboValue = 0;
		GameObject* modelObject = nullptr; // 非所有。数字モデルの親となる子GameObject（SceneBase::objects_が所有）
		float elapsed = 0.0f;              // このポップアップが生成されてからの経過秒数
	};

	// コンボが発生した瞬間に呼ぶ。表示中のポップアップがあれば（同じ値でも）即座に消して、
	// comboValueで新しくポップインをやり直す。実際のGameObject生成/破棄はSceneBase::
	// UpdateComboPopupComponentsが次フレームで処理する（pendingComboValue_に積むだけ。
	// IComponentはCreateObjectできないため）
	void RequestPopup(int comboValue) {
		pendingComboValue_ = comboValue;
		hasPendingRequest_ = true;
	}

	// SceneBase::UpdateComboPopupComponentsが、積まれたリクエストを取り出して処理するために使う。
	// 呼び出し後、内部の保留状態はクリアされる
	bool ConsumePendingRequest(int& outComboValue) {
		if (!hasPendingRequest_) return false;
		outComboValue = pendingComboValue_;
		hasPendingRequest_ = false;
		return true;
	}

	// 実行フェーズが終了した瞬間にPlayScene側が呼ぶ。表示中のポップアップを即座に消す
	// （まだ演出中でもフェード完了を待たず打ち切る）。実際の子GameObject破棄は
	// SceneBase::UpdateComboPopupComponentsが次フレームでactivePopup_の中身を見て行う
	void ClearAll() { clearRequested_ = true; }
	bool ConsumeClearRequested() {
		bool result = clearRequested_;
		clearRequested_ = false;
		return result;
	}

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// ---- 演出パラメータ（Inspectorで調整可能） ----

	// 頭上オフセット（プレイヤーのtranslationからの相対Y）
	float baseYOffset = 2.0f;

	// 数字1文字の基本サイズ（AlphabetTextComponent::charScale相当）
	float charScale = 0.8f;

	// 桁の中心から次の桁の中心までのX方向の距離（コンボ数値が2桁以上のときの数字同士の間隔）。
	// AlphabetTextComponent::charSpacingと同じ役割。charScaleに対して十分な余白を持たせないと
	// 桁同士が重なる（.objの実際の横幅次第で調整する）
	float digitSpacing = 0.96f; // 既定のcharScale(0.8)に対する従来のdigitSpacing=charScale*1.2の値

	// ポップイン（0→charScaleへ拡大）にかける時間(秒)
	float popInDuration = 0.15f;
	Easing::Type popInEasing = Easing::Type::kOutBack;

	// ポップイン完了後、次のコンボが来ないまま自動フェードアウトを始めるまでの静止表示時間(秒)。
	// この時間内に次のRequestPopupが来れば、静止表示中にカウントし直されてフェードアウトは起きない
	float holdDuration = 0.8f;

	// フェードアウト（alpha 1→0）にかける時間(秒)
	float fadeOutDuration = 0.4f;
	Easing::Type fadeOutEasing = Easing::Type::kInQuad;

	// SceneBase::UpdateComboPopupComponentsが管理する、現在表示中のポップアップ（無ければ
	// modelObject==nullptr）。保存対象外（演出中の一時状態のため、シーンをロードし直したら消えてよい）
	ActivePopup activePopup_;

private:
	int pendingComboValue_ = 0;
	bool hasPendingRequest_ = false;
	bool clearRequested_ = false;
};
