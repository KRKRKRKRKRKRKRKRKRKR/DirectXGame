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

	// nameをFactories()/Names()/GenericFlags()から取り除く。SceneBase::DrawSceneTransitionButtonsの
	// 削除UI専用。REGISTER_SCENE済みの固定シーン（.cppに実装が残っているクラス）については、
	// これだけでは実行中の一覧から消えるだけで、C++コード自体は残っているため通常は再起動すると
	// 復活する。永続的に消したい場合はこのメソッドの後にRecordPermanentlyDeleted(name)も呼ぶこと
	// （SceneBase::DeleteSceneFolderが両方を行う）
	static void Unregister(const std::string& name);

	// nameをResources/DeletedScenes.jsonへ追記する（既に記録済みなら何もしない）。
	// ApplyPermanentlyDeletedScenesが次回起動時にこの一覧を読み、該当する名前をUnregisterし直すことで、
	// REGISTER_SCENE済みの固定シーンであってもアプリ再起動後に一覧へ戻らないようにする
	// （C++クラス自体・Resources/{name}/のデータは削除しない。あくまでSceneRegistryへの
	// 再登録だけを永続的に抑止するブロックリスト）。SceneBase::DeleteSceneFolder専用
	static void RecordPermanentlyDeleted(const std::string& name);

	// Resources/DeletedScenes.jsonを読み込み、記載されている全名前をUnregisterする。
	// REGISTER_SCENEはmain()より前の静的初期化で必ず実行されるため、このメソッドは
	// その後（Game::InitializeがsceneManager_.Initialize()より前）に1回呼ぶ必要がある
	static void ApplyPermanentlyDeletedScenes();

	// oldNameで登録済みのファクトリ・GenericFlagsをnewNameへ丸ごと付け替える（登録順の位置も
	// 維持する）。oldNameが未登録、またはnewNameが既に登録済みの場合は何もせずfalseを返す。
	// SceneBase::DrawSceneTransitionButtonsの「名前変更」UI専用。呼び出し側がResources/{oldName}/を
	// Resources/{newName}/へ改名する処理（ファイルシステム操作）は別途自分で行うこと
	// （このメソッドはSceneRegistryの登録情報だけを付け替える）
	static bool Rename(const std::string& oldName, const std::string& newName);

	// GetAllNames()の並び順（Names()内でのindex）を1つ前/後ろの要素と入れ替える。
	// indexが範囲外、または端（MoveUpでindex==0、MoveDownで末尾）の場合は何もしない。
	// SceneBase::DrawSceneTransitionButtonsの「↑」「↓」ボタン専用
	static void MoveUp(size_t index);
	static void MoveDown(size_t index);

	// GetAllNames()の現在の並び順をResources/SceneOrder.jsonへ保存する。
	// SceneBase::DrawSceneTransitionButtonsが↑↓ボタンで並び替えるたびに呼ぶ
	static void SaveOrder();

	// Resources/SceneOrder.jsonを読み込み、記載されている順序でNames()を並べ替える。
	// ファイルに載っているがまだ未登録の名前（例：削除済みのGenericScene）は無視し、
	// 逆にファイルに載っていない登録済みの名前（新しく増えたシーン等）は末尾に追加する形で残す。
	// Game::InitializeがScanResourcesForGenericScenesの後に1回呼ぶ
	static void LoadOrder();

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
