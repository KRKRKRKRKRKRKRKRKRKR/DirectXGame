#pragma once
#include <string>

// SceneBaseをそのまま「空のシーン」として使う、C++クラス不要のシーンをResources/scenes.json
// で管理する。REGISTER_SCENEマクロ（.cppに1行書いてビルドし直す）とは別に、エディタの
// シーン作成/削除UI（SceneBase::DrawSceneTransitionButtons）から実行時にシーンを
// 増減できるようにするための仕組み。SceneRegistryは名前→ファクトリの単純なマップしか
// 持たないため、「どの名前がこのJSON管理下にあるか」の記録はこちらが持つ
class GenericSceneStore {
public:
	// 起動時（Game::Initializeの最初）に一度だけ呼ぶ。scenes.jsonを読み込み、記録されている
	// 全シーン名をSceneRegistry::Registerする。scenes.jsonが存在しない場合（このリセット後の
	// 初回起動）はデフォルトシーン"Main"を1つ自動生成して登録する。戻り値は起動時に
	// SceneManager::Initializeへ渡すべき開始シーン名
	static std::string LoadOrCreateDefault();

	// エディタの「+」ボタンから呼ぶ。名前バリデーション→重複チェック→SceneRegistry::Register→
	// Resources/{name}/フォルダ作成→scenes.jsonへ追記、の順に行う。
	// 成功時は空文字列、失敗時はユーザー向けのエラーメッセージを返す
	static std::string CreateScene(const std::string& name);

	// エディタの削除ボタンから呼ぶ。scenes.json管理下のシーンのみ対象（REGISTER_SCENEで
	// 登録されたC++シーンは対象外、"このシーンは削除できません"を返す）。
	// Resources/{name}/を再帰削除→SceneRegistryから登録解除→scenes.jsonから除去する。
	// 成功時は空文字列、失敗時はユーザー向けのエラーメッセージを返す
	static std::string DeleteScene(const std::string& name);
};
