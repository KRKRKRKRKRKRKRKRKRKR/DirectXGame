#pragma once
#include "GameObject.h"
#include "ComponentLoadContext.h"
#include <string>
#include <vector>
#include <memory>

// GameObject群のJSON保存・復元だけを担当するクラス（PlayScene本体からJSONの読み書きを分離するため）
class SceneSerializer {
public:
	static void Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects);

	// 成功時true。ファイルが開けない/JSONとして壊れている場合はobjectsを変更せずfalseを返す
	static bool Load(const std::string& path, std::vector<std::unique_ptr<GameObject>>& objects, const ComponentLoadContext& ctx);
};
