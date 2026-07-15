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
	// Add Componentメニュー（SceneBase::DrawAddComponentMenu）はこちらのリストだけを一覧表示する。
	// displayNameはInspector/Add ComponentメニューでのUI表示用（省略時はtypeNameがそのまま出る）。
	// typeName自体（JSON保存・ComponentRegistry::Create/RemoveByTypeNameの照合キー）は変えない
	template<typename T>
	static void RegisterSimple(const std::string& typeName, const std::string& displayName = "") {
		Register<T>(typeName,
			[](GameObject& obj, const ComponentLoadContext&, const nlohmann::json& data) {
				obj.template AddComponent<T>()->FromJson(data);
			},
			[](GameObject& obj) { return obj.template RemoveComponent<T>(); },
			displayName);
		SimpleTypeNames().push_back(typeName);
	}

	// コンストラクタ引数が要る、または兄弟コンポーネント/外部リソースに依存するコンポーネント用。
	// creator自身がAddComponent<T>(...)の呼び出し方を知っている前提。data={}での呼び出しは
	// 想定していない（ファイルパス未指定でのLoadModel失敗、兄弟コンポーネント未存在でのnullptr等、
	// 型ごとに個別の前提を満たす必要があるため、Add ComponentメニューではSimple系とは別に
	// 型ごとの専用UIを用意する）。
	// removerを渡すと、そのGetTypeName()に対してRemoveByTypeName経由の安全な削除ができるようになる
	// （TextureSelectorComponent等、依存先を持つ型の「先に依存先を消してもらう」ガードをここに書く）。
	// 省略した場合はRemoveByTypeNameで削除できない（呼び出し側が個別にRemoveComponent<T>()を呼ぶ前提のまま）。
	// displayNameはRegisterSimpleと同じくUI表示専用
	template<typename T>
	static void Register(const std::string& typeName, CreatorFunc creator, RemoverFunc remover = nullptr, const std::string& displayName = "") {
		Creators()[typeName] = std::move(creator);
		TypeNames()[std::type_index(typeid(T))] = typeName;
		if (remover) Removers()[typeName] = std::move(remover);
		DisplayNames()[typeName] = displayName.empty() ? typeName : displayName;
	}

	// typeNameに対応するcreatorを呼ぶ。未登録の型名の場合は何もしない
	static void Create(const std::string& typeName, GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data);

	// typeNameに対応する削除関数を呼ぶ（RegisterSimpleの型は自動登録、Register<T>の型は
	// remover引数を渡したものだけ）。見つからない・未登録・削除条件を満たさない場合はfalseを返す
	static bool RemoveByTypeName(const std::string& typeName, GameObject& obj);

	// 保存時にtypeid(*component)から型名を逆引きする。未登録なら空文字（＝保存対象外）を返す
	static std::string GetTypeName(const std::type_info& type);

	// typeNameに対応するUI表示名を返す（Inspectorの見出し・Add Componentコンボ用）。
	// displayName未登録ならtypeNameをそのまま返す
	static std::string GetDisplayName(const std::string& typeName);

	// RegisterSimpleで登録された型名の一覧（登録順）。Add Componentメニューのコンボに使う
	static const std::vector<std::string>& GetSimpleTypeNames() { return SimpleTypeNames(); }

private:
	static std::unordered_map<std::string, CreatorFunc>& Creators();
	static std::unordered_map<std::type_index, std::string>& TypeNames();
	static std::vector<std::string>& SimpleTypeNames();
	static std::unordered_map<std::string, RemoverFunc>& Removers();
	static std::unordered_map<std::string, std::string>& DisplayNames();
};
