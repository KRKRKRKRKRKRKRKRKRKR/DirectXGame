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
	// Resources/{assetFolder}/ui.json・Resources/{assetFolder}/scene.jsonへ保存する。
	// saveNameを指定すると、既定のui.json/scene.jsonとは別に
	// Resources/{assetFolder}/ui_{saveName}.json・scene_{saveName}.jsonという名前付き
	// スナップショットとして追加保存する（既定ファイルは上書きしない）
	static void Save(const std::string& assetFolder, const std::vector<std::unique_ptr<GameObject>>& objects,
		const std::string& saveName = "");

	// Resources/{assetFolder}/ui.json・scene.json（saveName指定時はui_{saveName}.json・
	// scene_{saveName}.json）を順にLoadしてobjectsへ合流させる（ui.jsonはclear、scene.jsonは追記）。
	// 1つでも読み込めればtrueを返す
	static bool Load(const std::string& assetFolder, std::vector<std::unique_ptr<GameObject>>& objects,
		const ComponentLoadContext& ctx, const std::string& saveName = "");
};
