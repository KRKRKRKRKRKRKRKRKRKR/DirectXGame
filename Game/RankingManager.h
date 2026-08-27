#pragma once
#include <string>
#include <vector>

// スコアランキングの読み書きを行うシングルトン。Resources/ranking.jsonに
// {"scores":[{"name":"...","score":...}, ...]}形式で保存する（GraviTwistBeta/ranking.jsonの
// 前例と同じ形式）。GameSessionと同じくアプリ全体で1つだけ生きているMeyerのシングルトン。
class RankingManager {
public:
	struct Entry {
		std::string name;
		int score = 0;
	};

	static RankingManager& GetInstance();

	// name/scoreを1件追加し、スコア降順でソートした上でSave()する。件数の上限は設けない
	// （全件を保持する）。ClearSceneが名前入力確定時に呼ぶ。戻り値はソート後、GetEntries()内で
	// このエントリが実際に位置するインデックス（RankingSceneが自分の順位への自動スクロール・
	// ハイライトに使う。GameSession::SetLastSubmittedEntryIndex参照）
	size_t Submit(const std::string& name, int score);

	// 未ロードならLoad()を1回だけ実行してから一覧を返す。ClearScene側がLoad()を明示的に
	// 呼び忘れても、初回アクセス時に自動で読み込まれるようにするためconstのままmutableで済ませる
	const std::vector<Entry>& GetEntries() const;

	// GetEntries()と同じ並び順のindex位置に対応する順位（1始まり）を返す。同スコアは同順位に
	// なり、その次の順位は同順位だった人数分だけ飛ぶ（スポーツ式。例：100点が2人なら1位・1位・
	// 3位）。RankingSceneがページ内の各行に表示する順位番号を求めるために使う
	int GetRank(size_t entryIndex) const;

	// 保存済みの全エントリを消去し、空の状態でSave()する（ranking.jsonも空のscores配列に
	// 上書きされる）。RankingSceneのリセットボタンから呼ぶ。取り消しはできない
	void Reset();

private:
	RankingManager() = default;

	mutable std::vector<Entry> entries_;
	mutable bool loaded_ = false;

	void EnsureLoaded() const;
	void Save() const;
};
