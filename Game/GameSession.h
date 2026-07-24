#pragma once
#include <string>

// シーンをまたいで持ち回りたい値（スコア・コンボ・入力済み名前）を保持するシングルトン。
// このエンジンはシーン切替のたびにGameObjectを全部作り直す設計（SceneManager::ChangeScene）
// のため、GameObject/Componentではシーン間でデータを引き継げない。AudioManager::GetInstance()
// と同じ「アプリ全体で1つだけ生きているオブジェクト」というMeyerのシングルトン方式を踏襲する。
//
// 中身のロジック（AddKillScore等のコンボ倍率計算）はフェーズ3で実装する。
// このフェーズでは骨格（値の保持とReset）のみを用意する。
class GameSession {
public:
	static GameSession& GetInstance();

	// Title→Tutorial遷移時などプレイ開始のたびに呼び、前回のプレイ結果を引きずらないようにする
	void Reset();

	int GetScore() const { return score_; }
	int GetCombo() const { return combo_; }
	const std::string& GetEnteredName() const { return enteredName_; }

private:
	GameSession() = default;

	int score_ = 0;
	int combo_ = 0;
	std::string enteredName_;
};
