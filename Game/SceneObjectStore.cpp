#include "SceneObjectStore.h"
#include "../Engine/GameObject/SceneSerializer.h"
#include "../Engine/GameObject/Component/TransformComponent.h"

namespace {
	std::string MakeUiPath(const std::string& assetFolder) {
		return "Resources/" + assetFolder + "/ui.json";
	}
	std::string MakeObjectPath(const std::string& assetFolder) {
		return "Resources/" + assetFolder + "/scene.json";
	}
}

void SceneObjectStore::Save(const std::string& assetFolder, const std::vector<std::unique_ptr<GameObject>>& objects) {
	// TransformComponent::is2D==trueをUI、それ以外をObjectとして2ファイルに振り分けて保存する
	SceneSerializer::Save(MakeUiPath(assetFolder), objects,
		[](GameObject& obj) { return obj.GetComponent<TransformComponent>()->is2D; });
	SceneSerializer::Save(MakeObjectPath(assetFolder), objects,
		[](GameObject& obj) { return !obj.GetComponent<TransformComponent>()->is2D; });
}

bool SceneObjectStore::Load(const std::string& assetFolder, std::vector<std::unique_ptr<GameObject>>& objects,
	const ComponentLoadContext& ctx) {
	// 呼び出し側（Loadボタン等）で何度呼ばれても既存のobjectsを毎回1回だけクリアしてから
	// ui.json/scene.jsonの内容で作り直す。ここでclear()しておかないと、2回目以降のLoadで
	// scene.json側（常にclearExisting=falseで追記）のオブジェクトが呼ぶたびに重複増殖する
	objects.clear();
	bool uiOk     = SceneSerializer::Load(MakeUiPath(assetFolder), objects, ctx, /*clearExisting=*/false);
	bool objectOk = SceneSerializer::Load(MakeObjectPath(assetFolder), objects, ctx, /*clearExisting=*/false);
	return uiOk || objectOk;
}
