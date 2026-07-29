#pragma once
#include "RenderComponentBase.h"
#include "TextureEntry.h"
#include <string>
#include <vector>
#include <functional>

// Model(OBJ)とFBXModelの両方に使う。ModelHandleを先頭引数に取るRenderer::DrawModelを
// 呼ぶ点が他の描画コンポーネントと異なる。hasAnimationが真の場合のみ、Draw()内で
// UpdateModelAnimationを呼んでからDrawModelする（ボーンアニメーション付きFBXModel用）。
//
// マルチマテリアル対応：Cube/Sphere/Sprite等はTextureSelectorComponent（別コンポーネント、
// テクスチャ1枚だけ選ぶ簡易版）でテクスチャを付けるが、Modelは複数のサブメッシュ
// （同じマテリアルを使う頂点範囲）を持ちうるため、サブメッシュ数ぶんのテクスチャ選択を
// このコンポーネント自身に内蔵している（TextureSelectorComponentはModelには使わない）
class ModelRenderComponent : public RenderComponentBase {
public:
	ModelRenderComponent(Renderer::ModelHandle handle, bool hasAnimation)
		: modelHandle(handle), hasAnimation(hasAnimation) {}

	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;
	void DrawImGui(const char* namePrefix) override;

	Renderer::ModelHandle modelHandle;
	bool                   hasAnimation;

	// modelHandleは実行のたびに変わる実行時ハンドルのため保存できない。JSONからの再読込に
	// Renderer::LoadModel(directoryPath, filename)を呼び直す必要があるため、元のパスを
	// 保持しておく（AddComponent直後にPlayScene側でセットする、コンストラクタ引数にはしない）
	std::string directoryPath;
	std::string filename;

	// サブメッシュごとのテクスチャ割り当て（textures_内でのindex、-1=未選択=白のまま）。
	// 要素数はDrawImGuiが実際のサブメッシュ数（Renderer::GetModelSubMeshCount）に
	// 合わせて自動でリサイズする
	std::vector<int> subMeshTextureIndices;

	// Inspectorのコンボ表示・サブメッシュ数の問い合わせに使う（TextureSelectorComponentと
	// 同じ「共有テクスチャ一覧を外部から注入する」パターン）。ComponentRegistration.cppの
	// creatorが生成直後にセットする。ensureTextureRegisteredは、プロジェクトパネルから
	// まだ登録されていない画像をサブメッシュのコンボへドラッグ&ドロップされた際、
	// SceneBase::EnsureTextureRegistered経由でtextures（実体はSceneBase::textures_と同じ
	// vector）へ追加登録するために使う
	void SetInspectorContext(Renderer* renderer, const std::vector<TextureEntry>* textures,
		std::function<void(const std::string&)> ensureTextureRegistered) {
		rendererForUi_ = renderer;
		textures_ = textures;
		ensureTextureRegistered_ = std::move(ensureTextureRegistered);
	}

	void ToJson(nlohmann::json& out) const override {
		RenderComponentBase::ToJson(out);
		out["directoryPath"] = directoryPath;
		out["filename"] = filename;
		out["hasAnimation"] = hasAnimation;

		// indexではなく名前で保存する（TextureSelectorComponentと同じ理由：テクスチャの
		// 読み込み順が変わってもindexがズレて別のテクスチャを指してしまわないようにするため）。
		// 未選択のサブメッシュはnullを入れる
		nlohmann::json names = nlohmann::json::array();
		for (int idx : subMeshTextureIndices) {
			if (textures_ && idx >= 0 && idx < static_cast<int>(textures_->size())) {
				names.push_back((*textures_)[idx].name);
			} else {
				names.push_back(nullptr);
			}
		}
		out["subMeshTextureNames"] = names;
	}
	void FromJson(const nlohmann::json& in) override {
		RenderComponentBase::FromJson(in);
		hasAnimation = in.value("hasAnimation", hasAnimation);

		subMeshTextureIndices.clear();
		if (in.contains("subMeshTextureNames")) {
			for (const auto& nameJson : in["subMeshTextureNames"]) {
				if (nameJson.is_null()) {
					subMeshTextureIndices.push_back(-1);
					continue;
				}
				std::string name = nameJson.get<std::string>();
				int resolved = -1;
				if (textures_) {
					for (size_t i = 0; i < textures_->size(); ++i) {
						if ((*textures_)[i].name == name) { resolved = static_cast<int>(i); break; }
					}
				}
				subMeshTextureIndices.push_back(resolved);
			}
		}
	}

private:
	Renderer* rendererForUi_ = nullptr;
	const std::vector<TextureEntry>* textures_ = nullptr;
	std::function<void(const std::string&)> ensureTextureRegistered_;
};
