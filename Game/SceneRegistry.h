#pragma once
#include "IScene.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// シーン名の文字列から対応するIScene派生クラスを生成するためのレジストリ。
// ComponentRegistry::RegisterSimple/REGISTER_SIMPLE_COMPONENTと同じ「名前ベースの自己登録」
// パターンを踏襲し、SceneManager.cpp側を手で編集しなくても新しいシーンを追加できるようにする
// （各シーン.cppの末尾にREGISTER_SCENEを1行書くだけでよい）
class SceneRegistry {
public:
	using Factory = std::function<std::unique_ptr<IScene>()>;

	// name: シーンの識別名。Resources/{name}/ をそのままアセットフォルダ名としても使う
	static void Register(const std::string& name, Factory factory);

	// 未登録の名前が渡された場合はnullptrを返す
	static std::unique_ptr<IScene> Create(const std::string& name);

	// 登録順のシーン名一覧（SceneBaseのシーン切替UI等、一覧表示したい呼び出し元向け）
	static const std::vector<std::string>& GetAllNames();

private:
	static std::unordered_map<std::string, Factory>& Factories();
	static std::vector<std::string>& Names();
};

// シーンクラスの.cppファイルの末尾に1行書くだけで、SceneManager.cppを編集しなくても
// 自動的に登録されるようにするマクロ。REGISTER_SIMPLE_COMPONENTと同じ、
// プログラム起動時（main()より前）の名前空間スコープ静的オブジェクトによる自己登録
#define REGISTER_SCENE(Type, name) \
	namespace { \
		struct Type##_SceneAutoRegister { \
			Type##_SceneAutoRegister() { SceneRegistry::Register(name, []() { return std::make_unique<Type>(); }); } \
		}; \
		Type##_SceneAutoRegister g_##Type##_sceneAutoRegister; \
	}
