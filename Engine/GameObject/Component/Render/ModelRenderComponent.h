#pragma once
#include "RenderComponentBase.h"
#include <string>

// Model(OBJ)とFBXModelの両方に使う。ModelHandleを先頭引数に取るRenderer::DrawModelを
// 呼ぶ点が他の描画コンポーネントと異なる。hasAnimationが真の場合のみ、Draw()内で
// UpdateModelAnimationを呼んでからDrawModelする（ボーンアニメーション付きFBXModel用）
class ModelRenderComponent : public RenderComponentBase {
public:
	ModelRenderComponent(Renderer::ModelHandle handle, bool hasAnimation)
		: modelHandle(handle), hasAnimation(hasAnimation) {}

	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;

	Renderer::ModelHandle modelHandle;
	bool                   hasAnimation;

	// modelHandleは実行のたびに変わる実行時ハンドルのため保存できない。JSONからの再読込に
	// Renderer::LoadModel(directoryPath, filename)を呼び直す必要があるため、元のパスを
	// 保持しておく（AddComponent直後にPlayScene側でセットする、コンストラクタ引数にはしない）
	std::string directoryPath;
	std::string filename;

	void ToJson(nlohmann::json& out) const override {
		RenderComponentBase::ToJson(out);
		out["directoryPath"] = directoryPath;
		out["filename"] = filename;
		out["hasAnimation"] = hasAnimation;
	}
	void FromJson(const nlohmann::json& in) override {
		RenderComponentBase::FromJson(in);
		hasAnimation = in.value("hasAnimation", hasAnimation);
	}
};
