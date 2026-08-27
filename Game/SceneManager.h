#pragma once
#include "IScene.h"
#include <memory>
#include <string>

class Renderer;
class Camera;

// 現在のSceneを1つだけ保持し、IScene::GetNextScene()の戻り値を見て毎フレーム切り替えを行う。
// 複数シーンの同時ロードや先読み等は行わない、常に1つを生成・破棄する単純な方式にしている。
// どのシーン名がどの具象クラスに対応するかはSceneRegistryが持つため、ここでは関知しない
class SceneManager {
public:
	void Initialize(Renderer* renderer, Camera* camera, const std::string& startScene);
	void Render(float deltaTime);

	// アプリ終了確認等、現在のシーンに対して外部から操作したい呼び出し元向け
	// （Game::Updateがアプリ終了時の保存確認でRequestSave()を呼ぶ）
	IScene* GetCurrentScene() const { return currentScene_.get(); }

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	std::unique_ptr<IScene> currentScene_;

	// 現在のシーンがGetNextScene()で遷移を要求してから、実際にChangeSceneするまでの間、
	// FadeManagerのフェードイン演出を待つために遷移先の名前を保持しておく場所。空文字列は
	// 「遷移待機中ではない」を意味する（IScene::GetNextScene()の「空文字列=遷移しない」と
	// 同じ規約を踏襲する）
	std::string pendingNextScene_;

	// 現在のcurrentScene_のInitialize()（シーンJSON読込・GameObject/アセット生成）が完了して
	// いるかどうかの読み込み完了フラグ。ChangeScene()の最後、Initialize()の呼び出しが実際に
	// 返ってきた直後にtrueへ立てる（同期読み込みのため、この時点で本当に読み込みは終わっている）。
	// ChangeScene()を呼んだ直後（次のフェードアウトを始めてよいと判定する前）は必ずfalseから
	// スタートする。フェードアウトはこのフラグがtrueになったフレームで初めて開始することで、
	// 「シーン切替の重い処理でdeltaTimeが跳ね上がり、フェードアウト演出が1フレームで
	// スキップされて旧シーンの残像が一瞬見える」という問題を、deltaTimeのクランプではなく
	// 実際の読み込み完了を確認する形で根本的に解消する
	bool sceneReady_ = false;

	void ChangeScene(const std::string& next);
};
