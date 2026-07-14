#pragma once
#include "GameObject.h"
#include "ComponentLoadContext.h"
#include "../../Externals/Json/json.hpp"
#include <functional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// 「JSONに書かれた型名の文字列」から正しい具象コンポーネントを生成するためのレジストリ。
// GameObjectは自分が何のコンポーネントを持っているか実行時までわからないため、
// ロード時にどの具象クラスをnewするかを名前ベースで解決する必要がある
// （RenderComponentFactoryのenum版を、全コンポーネント種・名前ベースに一般化したもの）。
// 実際の登録はComponentRegistration.cppのRegisterEngineComponents()に集約する
class ComponentRegistry {
public:
	using CreatorFunc = std::function<void(GameObject&, const ComponentLoadContext&, const nlohmann::json&)>;
	using RemoverFunc = std::function<bool(GameObject&)>;

	// デフォルト構築でき、他コンポーネントや外部リソースに依存しない「単純な」コンポーネント用。
	// AddComponent<T>()した直後にFromJson(data)を呼ぶだけの定型処理を1行で登録できる。
	// data={}（空JSON）で呼んでも安全に既定値のコンポーネントが付与されるため、
	// Add Componentメニュー（SceneBase::DrawAddComponentMenu）はこちらのリストだけを一覧表示する
	template<typename T>
	static void RegisterSimple(const std::string& typeName) {
		Register<T>(typeName, [](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
			obj.template AddComponent<T>()->FromJson(data);
		});
		SimpleTypeNames().push_back(typeName);
		Removers()[typeName] = [](GameObject& obj) { return obj.template RemoveComponent<T>(); };
	}

	// コンストラクタ引数が要る、または兄弟コンポーネント/外部リソースに依存するコンポーネント用。
	// creator自身がAddComponent<T>(...)の呼び出し方を知っている前提。data={}での呼び出しは
	// 想定していない（ファイルパス未指定でのLoadModel失敗、兄弟コンポーネント未存在でのnullptr等、
	// 型ごとに個別の前提を満たす必要があるため、Add ComponentメニューではSimple系とは別に
	// 型ごとの専用UIを用意する）
	template<typename T>
	static void Register(const std::string& typeName, CreatorFunc creator) {
		Creators()[typeName] = std::move(creator);
		TypeNames()[std::type_index(typeid(T))] = typeName;
	}

	// typeNameに対応するcreatorを呼ぶ。未登録の型名の場合は何もしない
	static void Create(const std::string& typeName, GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data);

	// typeNameに対応するRemoveComponent<T>()を呼ぶ（RegisterSimpleの型のみ登録されている。
	// 依存ありのカスタム登録型はSceneBase::DrawAddComponentMenuが個別にRemoveComponent<T>()を呼ぶ）。
	// 見つからない・未登録ならfalseを返す
	static bool RemoveByTypeName(const std::string& typeName, GameObject& obj);

	// 保存時にtypeid(*component)から型名を逆引きする。未登録なら空文字（＝保存対象外）を返す
	static std::string GetTypeName(const std::type_info& type);

	// RegisterSimpleで登録された型名の一覧（登録順）。Add Componentメニューのコンボに使う
	static const std::vector<std::string>& GetSimpleTypeNames() { return SimpleTypeNames(); }

private:
	static std::unordered_map<std::string, CreatorFunc>& Creators();
	static std::unordered_map<std::type_index, std::string>& TypeNames();
	static std::vector<std::string>& SimpleTypeNames();
	static std::unordered_map<std::string, RemoverFunc>& Removers();
};
