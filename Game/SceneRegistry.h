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

	// nameが未登録なら、GenericScene（SceneBaseそのまま、何もオーバーライドしない空シーン）の
	// ファクトリとして動的登録する。既に登録済み（REGISTER_SCENEの固定シーン、または過去に
	// このメソッドで登録済みのGenericScene）の場合は何もしない（falseを返す）。
	// Game::Initialize起動時のResources/走査（既存の名前付きシーンフォルダを一覧に復元するため）と、
	// SceneBase::DrawSceneTransitionButtonsの「新規シーン作成」UIの両方から呼ばれる
	static bool RegisterGenericIfMissing(const std::string& name);

	// nameが既にFactories()に登録済みかどうか（RegisterGenericIfMissingの重複防止・
	// 新規シーン作成UIの名前バリデーションに使う）
	static bool IsRegistered(const std::string& name);

	// 未登録の名前が渡された場合はnullptrを返す
	static std::unique_ptr<IScene> Create(const std::string& name);

	// 登録順のシーン名一覧（SceneBaseのシーン切替UI等、一覧表示したい呼び出し元向け）
	static const std::vector<std::string>& GetAllNames();

	// Resources/直下の各フォルダを走査し、scene.jsonまたはui.jsonを持つがまだ未登録の名前が
	// あればRegisterGenericIfMissingで動的登録する。REGISTER_SCENE済みの固定シーン（Title/Play等）は
	// 既に登録済みのためスキップされ、過去に「新規シーン作成」で作ったGenericScene名だけが
	// 再起動のたびにここで復元される。Game::InitializeがsceneManager_.Initialize()より前に1回呼ぶ
	static void ScanResourcesForGenericScenes();

	// nameがREGISTER_SCENE（固定シーン、.cppファイルに実装されたクラス）ではなく、
	// RegisterGenericIfMissing/ScanResourcesForGenericScenesで動的登録されたGenericSceneかどうか。
	// SceneBase::DrawSceneTransitionButtonsの削除UIが、削除後に一覧からも消すべきか
	// （GenericSceneならUnregisterも呼ぶ）を判断するために使う
	static bool IsGeneric(const std::string& name);

	// nameをFactories()/Names()から取り除く。IsGeneric(name)がtrueの場合のみ呼ぶこと
	// （REGISTER_SCENEの固定シーンを外すと、次にそのシーンへ切り替えようとした際に
	// 二度と復元できなくなるため。SceneBase::DrawSceneTransitionButtonsの削除UI専用）
	static void Unregister(const std::string& name);

private:
	static std::unordered_map<std::string, Factory>& Factories();
	static std::vector<std::string>& Names();
	// RegisterGenericIfMissing/ScanResourcesForGenericScenesで動的登録された名前の集合
	// （IsGeneric/Unregisterの判定に使う）
	static std::unordered_map<std::string, bool>& GenericFlags();
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
