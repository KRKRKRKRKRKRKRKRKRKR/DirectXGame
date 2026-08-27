#pragma once
#include <string>

// シーンをまたいで持ち回りたい値（スコア・入力済み名前）を保持するシングルトン。
// このエンジンはシーン切替のたびにGameObjectを全部作り直す設計（SceneManager::ChangeScene）
// のため、GameObject/Componentではシーン間でデータを引き継げない。AudioManager::GetInstance()
// と同じ「アプリ全体で1つだけ生きているオブジェクト」というMeyerのシングルトン方式を踏襲する。
//
// PlayScene::score_（1体倒すごとにcomboCount_を加算した累計）がラウンド消化完了で
// クリア確定した瞬間、PlayScene::UpdateExecutionPhaseStatsがSetScore()で最終スコアを渡す。
// ClearSceneはそれをGetScore()で読み、名前入力後にRankingManager::Submitへ渡す
class GameSession {
public:
	static GameSession& GetInstance();

	// Title→Tutorial遷移時などプレイ開始のたびに呼び、前回のプレイ結果を引きずらないようにする
	void Reset();

	void SetScore(int score) { score_ = score; }
	int GetScore() const { return score_; }

	void SetEnteredName(const std::string& name) { enteredName_ = name; }
	const std::string& GetEnteredName() const { return enteredName_; }

	// ClearSceneがNextボタン押下時、RankingManager::Submitの戻り値（GetEntries()内での
	// 挿入位置）をここへ渡す。RankingSceneが初回表示時にConsumeLastSubmittedEntryIndex()で
	// 読み取り、自分の順位まで自動スクロール＋ハイライトする（詳しくはRankingScene参照）
	void SetLastSubmittedEntryIndex(size_t index) { lastSubmittedEntryIndex_ = static_cast<int>(index); }

	// 一度だけ消費できる値。呼び出し後は-1に戻るため、Titleの「ランキングを見る」ボタンから
	// 再度Rankingへ入った場合など、古い情報で誤ってハイライトされることを防ぐ。
	// 未設定（-1）ならstd::nullopt相当としてhasValueをfalseで返す
	bool ConsumeLastSubmittedEntryIndex(size_t& outIndex) {
		if (lastSubmittedEntryIndex_ < 0) return false;
		outIndex = static_cast<size_t>(lastSubmittedEntryIndex_);
		lastSubmittedEntryIndex_ = -1;
		return true;
	}

private:
	GameSession() = default;

	int score_ = 0;
	std::string enteredName_;
	int lastSubmittedEntryIndex_ = -1;
};
