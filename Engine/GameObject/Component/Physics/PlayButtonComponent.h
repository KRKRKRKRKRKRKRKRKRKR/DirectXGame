#pragma once
#include "../../IComponent.h"
#include "../Audio/ProjectAssetEntry.h"
#include "../../../../Math/MathTypes.h"
#include <vector>
#include <string>

// タイトル画面のPLAYボタン。OBBColliderComponentと同じGameObjectに付ける
// （そのOBBを当たり判定として使う）。毎フレーム、自分のOBBColliderComponentとマウスレイの
// 交差を判定し、IsHovering()で結果を公開する。左クリックされた瞬間はSEを鳴らし、
// clicked_フラグを立てる。
//
// 見た目の反映（ホバー中の色・拡大率）はこのコンポーネント自身が行う：自分のGameObjectの
// 「子」に付いているAlphabetTextComponent全てへ、毎フレーム自動的にdisplayColor/
// displayScaleMultiplierを書き込む。つまり「PLAY」等の見た目役GameObjectをこのコンポーネントが
// 付いたGameObjectの子にするだけでホバー演出が効き、Game層でタグ名を対応させる必要が無い
// （旧方式：SceneBase::UpdateButtonAndReflectHoverがhitboxタグ・textタグの2つを個別に
// 探して手動で反映していた。既存シーン（TitleScene等）はこの旧方式のまま親子関係を持たせて
// いないため、そちらは今まで通りUpdateButtonAndReflectHoverを使う。新しく作るボタンは
// このコンポーネントのGameObjectの子にAlphabetTextComponentを置くだけでよい）。
// シーン遷移（クリック後どこへ行くか）は引き続きScene側がConsumeClicked()を読んで決める
// （Engine層のこのコンポーネントがGame層のシーン遷移先を知る必要が無いようにするための分離）
class PlayButtonComponent : public IComponent {
public:
	// audioClips: SceneBase::projectAudioClips_への非所有ポインタ（ComponentLoadContext::audioClips）。
	// クリック音は未設定（initialIndex=-1）でも動作する（HitSoundComponentと同じ「鳴らせるものが
	// 無ければ何もしない」方式。後日SEファイルを追加してからInspectorで選べばよい）
	PlayButtonComponent(const std::vector<ProjectAssetEntry>* audioClips, int initialIndex = -1, float volume = 1.0f);

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// TitleScene::HandleSceneTransitionInputが毎フレーム呼ぶ。クリックされていればtrueを返し、
	// 呼び出し後はフラグを消費する（ワンショット。ReflexPlayerComponent::ConsumeExecutionFinished
	// と同じパターン）
	bool ConsumeClicked() {
		bool result = clicked_;
		clicked_ = false;
		return result;
	}

	bool IsHovering() const { return isHovering_; }

	// false の間はホバー判定・クリック判定を一切行わない（IsHovering()は常にfalse、
	// ConsumeClicked()は常にfalseを返す）。ClearSceneのNextボタン（名前未入力の間は
	// 押せないようにしたい）等、呼び出し側が実行時に毎フレーム条件で切り替える用途を想定した
	// 実行時専用フラグのためJSONには保存しない（既定はtrue＝常時有効、PlayButton等の
	// 従来の使い方はこれまで通り変更なしで動く）
	bool enabled = true;

	// ---- ホバー演出パラメータ（Inspectorで調整可能。TitleScene側がIsHovering()と合わせて参照し、
	// 対象のAlphabetTextComponentへ反映する） ----
	Vector4 normalColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 hoverColor  = { 1.0f, 0.9f, 0.2f, 1.0f };
	float   normalScaleMultiplier = 1.0f;
	float   hoverScaleMultiplier  = 1.1f;

	// enabled==falseのとき、ホバー色の代わりにこの色を使いたい呼び出し側向け（例：Nextボタンを
	// 押せない間はグレー表示にする）。TitleScene::PlayButton等、既存の使い方には影響しない
	// （呼び出し側がIsHovering()と一緒にenabledも見て、自分でどの色を使うか決める）
	Vector4 disabledColor = { 0.4f, 0.4f, 0.4f, 1.0f };

private:
	const std::vector<ProjectAssetEntry>* audioClips_;
	int   audioIndex_;
	float volume_;

	bool prevMouseLeftPressed_ = false;
	bool clicked_ = false;
	bool isHovering_ = false;
};
