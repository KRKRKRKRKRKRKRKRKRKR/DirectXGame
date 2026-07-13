#pragma once
#include "GameObject.h"
#include "ComponentLoadContext.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

// GameObject群のJSON保存・復元だけを担当するクラス（PlayScene本体からJSONの読み書きを分離するため）
class SceneSerializer {
public:
	static void Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects);

	// filterがtrueを返したオブジェクトだけを保存する（UI用/Object用ファイル分割等、
	// objects_を実際に分割せず「保存対象を選ぶ条件」だけ変えたい場合に使う）。
	// GameObject::GetComponent<T>()が非constのためfilterの引数も非const参照にしている
	static void Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects,
		const std::function<bool(GameObject&)>& filter);

	// 成功時true。ファイルが開けない/JSONとして壊れている場合はobjectsを変更せずfalseを返す。
	// clearExisting=falseにすると既存のobjectsを消さずに末尾へ追記する（UI用/Object用など、
	// 複数ファイルを順番にLoadして1つのobjects_へ合流させたい場合に使う）
	static bool Load(const std::string& path, std::vector<std::unique_ptr<GameObject>>& objects,
		const ComponentLoadContext& ctx, bool clearExisting = true);
};
