#pragma once
#include "../Engine/GameObject/GameObject.h"
#include "../Engine/GameObject/ComponentLoadContext.h"
#include <memory>
#include <string>
#include <vector>

// SceneBaseのobjects_をUI(is2D)/Object(3D)に振り分けてJSONへ保存・復元するだけの責務を
// 切り出したクラス。SceneBase::SaveScene/LoadSceneが行っていたファイルパス組み立て・
// SceneSerializer呼び出しをここに集約し、SceneBase自体の行数・責務を減らす
class SceneObjectStore {
public:
	// objectsをis2D（UI）/それ以外（Object）で振り分けて、
	// Resources/{assetFolder}/ui.json・Resources/{assetFolder}/scene.jsonへ保存する
	static void Save(const std::string& assetFolder, const std::vector<std::unique_ptr<GameObject>>& objects);

	// Resources/{assetFolder}/ui.json・scene.jsonを順にLoadしてobjectsへ合流させる
	// （ui.jsonはclear、scene.jsonは追記）。1つでも読み込めればtrueを返す
	static bool Load(const std::string& assetFolder, std::vector<std::unique_ptr<GameObject>>& objects,
		const ComponentLoadContext& ctx);
};
