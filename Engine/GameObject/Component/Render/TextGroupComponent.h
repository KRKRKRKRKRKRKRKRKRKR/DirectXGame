// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "TextSpriteComponent.h"
#include "../../../../Math/MathTypes.h"
#include <string>
#include <vector>

// 複数のTextSpriteComponentをまとめて管理する「設定のみ」コンポーネント（AlphabetTextComponentと
// 同じ方針）。このコンポーネント自身はGameObjectを生成する権限を持たない（IComponentはシーンを
// 知らない）ため、実際にエントリごとの子GameObject（TextSpriteComponent付き）を生成・破棄するのは
// SceneBase::UpdateTextGroupComponents（毎フレーム、Render()から呼ばれる）が担当する。
// このコンポーネントはentries（文字列リスト）等の「設定」と、SceneBase側が前回何を生成したかを
// 検知するための内部状態だけを持つ。
//
// 用途はまだ決まっていない（文字列リストを持ち、変更を検知して子のTextSpriteComponentを
// 自動生成・自動配置する、という仕組みだけを先に用意したもの）。具体的な使い道が決まったら、
// 呼び出し側（各Scene）がentriesを書き換えるだけで表示内容を制御できる想定
class TextGroupComponent : public IComponent {
public:
	// 1エントリ＝1つのTextSpriteComponentに対応する設定
	struct Entry {
		std::string text;
		float fontSize = 32.0f;
		TextSpriteComponent::HorizontalAlign horizontalAlign = TextSpriteComponent::HorizontalAlign::kCenter;
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		bool operator==(const Entry& other) const {
			return text == other.text
				&& fontSize == other.fontSize
				&& horizontalAlign == other.horizontalAlign
				&& color.x == other.color.x && color.y == other.color.y
				&& color.z == other.color.z && color.w == other.color.w;
		}
		bool operator!=(const Entry& other) const { return !(*this == other); }
	};

	std::vector<Entry> entries;

	// 1個目のテキストの位置。オーナー（このコンポーネントが付いたGameObject）のtranslationからの
	// 相対オフセット。オーナー自身がTransformComponent::is2D==trueのスクリーン空間GameObject
	// （px、左上原点）である想定
	Vector3 anchorOffset = { 0.0f, 0.0f, 0.0f };

	// エントリ間の間隔(px)
	float spacing = 40.0f;

	enum class StackDirection { kVertical, kHorizontal };
	StackDirection stackDirection = StackDirection::kVertical;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// SceneBase::UpdateTextGroupComponentsが「前回子GameObjectを組み立てた時点の値」を控えておく
	// ために使う（AlphabetTextComponent::lastBuiltText等と同じ役割）。entries/anchorOffset/spacing/
	// stackDirectionのいずれかが今の値と食い違っていたら子GameObjectを作り直す。builtOnceは初回を
	// 必ず「食い違い」扱いにするためのフラグ（Vector3/vectorに「あり得ない番兵値」を作るより単純）
	bool builtOnce = false;
	std::vector<Entry> lastBuiltEntries;
	Vector3 lastBuiltAnchorOffset = { 0.0f, 0.0f, 0.0f };
	float lastBuiltSpacing = 0.0f;
	StackDirection lastBuiltStackDirection = StackDirection::kVertical;
};
