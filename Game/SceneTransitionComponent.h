#pragma once
#include "../Engine/GameObject/IComponent.h"
#include <string>

// 「キー入力またはボタンクリックで指定シーンへ遷移する」を汎用化したコンポーネント。
// 従来は各シーンサブクラス(TitleScene/RankingScene等)のHandleSceneTransitionInput()に
// 個別に書かれていたが、SceneBase自体をそのまま「空のシーン」として使う新しい
// 汎用シーン(GenericSceneStore参照)にはそのオーバーライドが無いため、遷移トリガーを
// GameObject側のデータとして持たせられるようにする。
//
// SceneRegistry::GetAllNames()(遷移先の選択コンボ)を直接参照するため、Engine層ではなく
// Game層に置く（PlayButtonComponent.hのコメント通り、Engine層のコンポーネントはGame層の
// シーン名/タグ名を知らない設計を踏襲するため。Engine/GameObject/Component配下に
// Game/*.hをincludeしているファイルは存在しない）
class SceneTransitionComponent : public IComponent {
public:
	bool enabled = true;

	// 遷移先シーン名。DrawImGuiでSceneRegistry::GetAllNames()から選ばせるため、
	// 手打ちでの綴り間違いは基本的に起きない
	std::string targetScene;

	// キー入力トリガー。0はDIK_ESCAPE等の割り当てなし（無効）を表す
	int triggerKey = 0;

	// ボタンクリックトリガー。SceneBase::UpdateButtonAndReflectHover(hitboxTag, textTag)と
	// 同じ2タグ方式（hitboxTagのGameObjectにOBBColliderComponent+PlayButtonComponentが
	// 必要。textTagは見た目のホバー反映先で省略可）
	bool useButtonClick = false;
	std::string hitboxTag;
	std::string textTag;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
