#include "SceneManager.h"
#include "SceneRegistry.h"
#include "FadeManager.h"
#include "../Engine/Utils/Logger.h"

void SceneManager::Initialize(Renderer* renderer, Camera* camera, const std::string& startScene) {
	renderer_ = renderer;
	camera_ = camera;
	ChangeScene(startScene);
}

void SceneManager::Render(float deltaTime) {
	// currentScene_はChangeScene()がSceneRegistry::Createの失敗(未登録のシーン名)時にnullptrの
	// ままになりうる（Logger::Logで警告済み）。何もせず抜けることで、シーンが1つも無い/名前を
	// 間違えた状態でも即クラッシュせず、次のフレーム以降に正しい名前で遷移し直せるようにする
	if (!currentScene_) return;

	currentScene_->Render(deltaTime);

	// Render完了後に遷移要求を確認する（描画中にシーンが差し替わらないようにするため）。
	// ここで直接ChangeSceneせず、まずFadeManagerのフェードインを開始して遷移先を
	// pendingNextScene_に退避しておく。フェードインが完了（画面全体が覆われた＝
	// FadeManager::IsCovered()）した瞬間に実際の切替を行うことで、「暗転→切替→暗転から復帰」
	// という一連の演出をシーン側の実装に一切触れずに実現する
	if (pendingNextScene_.empty()) {
		std::string next = currentScene_->GetNextScene();
		if (!next.empty()) {
			pendingNextScene_ = next;
			// sceneReady_は「今待っている遷移についてChangeSceneが完了したか」を表すフラグの
			// つもりだが、実体は前回の遷移でtrueになったまま使い回されるメンバ変数のため、
			// ここで明示的にfalseへ戻しておかないと、2回目以降の遷移で「まだChangeSceneして
			// いないのにsceneReady_==trueのまま」という不整合が起き、ChangeSceneが一度も
			// 呼ばれずにStartFadeOut()だけが呼ばれてしまう（＝シーンが切り替わらないまま
			// フェードだけ繰り返される不具合の直接原因だった）
			sceneReady_ = false;
			FadeManager::GetInstance().StartFadeIn();
		}
	} else if (FadeManager::GetInstance().IsCovered()) {
		if (!sceneReady_) {
			// まだChangeSceneを実行していない（このelse ifブロックに入った最初のフレーム）。
			// pendingNextScene_はChangeScene完了後もsceneReady_の判定に使うため、ここではまだ
			// クリアしない
			ChangeScene(pendingNextScene_);
			// ChangeScene()の中でsceneReady_がtrueになる。フェードアウトの開始は次のRender呼び出し
			// （＝新シーンの初回フレーム）まで1フレーム遅らせることで、シーン切替の重い処理が
			// 終わったことを確実に確認してから演出を進める
		} else {
			// 新シーンの読み込みが実際に完了していることを確認できたので、ここで初めて
			// フェードアウトを開始する。sceneReady_はChangeScene()の最後で立てる（詳細は
			// SceneManager.hのコメント参照）
			pendingNextScene_.clear();
			FadeManager::GetInstance().StartFadeOut();
		}
	}
}

void SceneManager::ChangeScene(const std::string& next) {
	auto scene = SceneRegistry::Create(next);
	if (!scene) {
		Logger::Log("SceneManager: 未登録のシーン名が指定されました: " + next);
		return;
	}
	sceneReady_ = false;
	currentScene_ = std::move(scene);
	// アセットフォルダ名はシーン名をそのまま使う（Resources/{シーン名}/）。この呼び出しが
	// 返ってきた時点で、シーンJSON読込・GameObject/アセット生成はすべて完了している
	// （同期処理のため）。よってここで即座にsceneReady_をtrueにしてよい
	currentScene_->Initialize(renderer_, camera_, next);
	sceneReady_ = true;
}
