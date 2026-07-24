#include "SceneObjectStore.h"
#include "../Engine/GameObject/SceneSerializer.h"
#include "../Engine/GameObject/Component/TransformComponent.h"

namespace {
	// saveNameが空なら既定のui.json/scene.json、指定時はui_{saveName}.json/scene_{saveName}.json
	// という名前付きスナップショットのパスを組み立てる
	std::string MakeUiPath(const std::string& assetFolder, const std::string& saveName) {
		return "Resources/" + assetFolder + "/ui" + (saveName.empty() ? "" : "_" + saveName) + ".json";
	}
	std::string MakeObjectPath(const std::string& assetFolder, const std::string& saveName) {
		return "Resources/" + assetFolder + "/scene" + (saveName.empty() ? "" : "_" + saveName) + ".json";
	}
}

void SceneObjectStore::Save(const std::string& assetFolder, const std::vector<std::unique_ptr<GameObject>>& objects,
	const std::string& saveName) {
	// TransformComponent::is2D==trueをUI、それ以外をObjectとして2ファイルに振り分けて保存する
	SceneSerializer::Save(MakeUiPath(assetFolder, saveName), objects,
		[](GameObject& obj) { return obj.GetComponent<TransformComponent>()->is2D; });
	SceneSerializer::Save(MakeObjectPath(assetFolder, saveName), objects,
		[](GameObject& obj) { return !obj.GetComponent<TransformComponent>()->is2D; });
}

bool SceneObjectStore::Load(const std::string& assetFolder, std::vector<std::unique_ptr<GameObject>>& objects,
	const ComponentLoadContext& ctx, const std::string& saveName) {
	// 呼び出し側（Loadボタン等）で何度呼ばれても既存のobjectsを毎回1回だけクリアしてから
	// ui.json/scene.jsonの内容で作り直す。ここでclear()しておかないと、2回目以降のLoadで
	// scene.json側（常にclearExisting=falseで追記）のオブジェクトが呼ぶたびに重複増殖する
	objects.clear();
	bool uiOk     = SceneSerializer::Load(MakeUiPath(assetFolder, saveName), objects, ctx, /*clearExisting=*/false);
	bool objectOk = SceneSerializer::Load(MakeObjectPath(assetFolder, saveName), objects, ctx, /*clearExisting=*/false);
	return uiOk || objectOk;
}
