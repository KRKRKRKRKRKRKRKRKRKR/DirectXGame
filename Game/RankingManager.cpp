#include "RankingManager.h"
#include "../Externals/Json/json.hpp"
#include "../Engine/Utils/Logger.h"
#include <algorithm>
#include <fstream>

namespace {
constexpr const char* kRankingPath = "Resources/ranking.json";
}

RankingManager& RankingManager::GetInstance() {
	static RankingManager instance;
	return instance;
}

void RankingManager::EnsureLoaded() const {
	if (loaded_) return;
	loaded_ = true; // ファイルが無い/壊れている場合も再読込を繰り返さないよう先に立てる

	std::ifstream in(kRankingPath, std::ios::binary);
	if (!in.is_open()) return; // 初回起動時はファイルが存在しないのが正常なケース

	nlohmann::json j;
	try {
		in >> j;
	} catch (const std::exception& e) {
		Logger::Log(std::string("RankingManager: ranking.jsonの解析に失敗しました: ") + e.what());
		return;
	}

	if (!j.contains("scores") || !j["scores"].is_array()) return;
	for (const auto& item : j["scores"]) {
		Entry entry;
		entry.name = item.value("name", std::string());
		entry.score = item.value("score", 0);
		entries_.push_back(entry);
	}

	// Submitはentries_が常にスコア降順ソート済みという不変条件のもとstd::upper_boundで挿入位置を
	// 求める設計のため、ファイル側が何らかの理由で降順になっていない場合（手動編集等）に備えて
	// ロード直後に一度ソートしておく
	std::stable_sort(entries_.begin(), entries_.end(),
		[](const Entry& a, const Entry& b) { return a.score > b.score; });
}

void RankingManager::Save() const {
	nlohmann::json j;
	nlohmann::json scores = nlohmann::json::array();
	for (const auto& entry : entries_) {
		scores.push_back({ {"name", entry.name}, {"score", entry.score} });
	}
	j["scores"] = scores;

	std::ofstream out(kRankingPath, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		Logger::Log("RankingManager: ranking.jsonの書き込みに失敗しました");
		return;
	}
	out << j.dump(4);
}

size_t RankingManager::Submit(const std::string& name, int score) {
	EnsureLoaded();

	// entries_は常にスコア降順にソート済みという不変条件を保っているため、全体をsortし直さず
	// std::upper_boundで挿入位置を直接求める（安定ソート相当：同スコアの既存エントリより後ろに
	// 挿入されるため、既存エントリ同士の相対順序も変わらない）。挿入位置がそのまま戻り値になる
	// （sortによる要素の再配置後にアドレス/値で位置を探し直す必要がなく、確実に「今追加した
	// エントリ」のインデックスを返せる）
	auto it = std::upper_bound(entries_.begin(), entries_.end(), score,
		[](int value, const Entry& e) { return value > e.score; });
	size_t index = static_cast<size_t>(it - entries_.begin());
	entries_.insert(it, Entry{ name, score });

	Save();
	return index;
}

const std::vector<RankingManager::Entry>& RankingManager::GetEntries() const {
	EnsureLoaded();
	return entries_;
}

void RankingManager::Reset() {
	EnsureLoaded();
	entries_.clear();
	Save();
}

int RankingManager::GetRank(size_t entryIndex) const {
	EnsureLoaded();
	if (entryIndex >= entries_.size()) return 0;

	// 自分より前に、自分と異なるスコアが何件あるかを数える。entries_はスコア降順ソート済みのため、
	// 同スコアの一群は必ず連続している＝直前のエントリとスコアが違う位置まで遡ればよい
	size_t rank = entryIndex + 1;
	while (rank > 1 && entries_[rank - 2].score == entries_[entryIndex].score) {
		--rank;
	}
	return static_cast<int>(rank);
}
