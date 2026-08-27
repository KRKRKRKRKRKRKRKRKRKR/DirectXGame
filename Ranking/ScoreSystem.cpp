#include "ScoreSystem.h"
#include <imgui.h>
//public-------------------------------------------------------------

ScoreSystem::ScoreSystem() : fileName("ranking.json") {
	// 起動時に一回読み込んでおく
	scores_ = Load();
	camera.InitCameraTransform(cameraInfo, 1280.0f, 720.0f);
	playerName.clear();
}

void ScoreSystem::InputName(char* keys, char* preKeys) {
	if (keys[DIK_BACK] && !preKeys[DIK_BACK]) {
		if (!playerName.empty()) {
			playerName.pop_back();
		}
	}

	// 2. アルファベットの入力 (1つのループにまとめる)
	for (int i = 0; i < 26; i++) {
		int code = upperKeyMaps[i].dikCode; // DIK_A など

		if (keys[code] && !preKeys[code]) { // キーが押された瞬間
			if (playerName.length() < nameLengthLimit) {
				// Shiftが押されているかチェック
				if (keys[DIK_LSHIFT] || keys[DIK_RSHIFT]) {
					playerName += upperKeyMaps[i].character; // 大文字
				}
				else {
					playerName += lowKeyMaps[i].character;   // 小文字
				}
			}
		}
	}

	// 3. 数字の入力
	for (const auto& km : numKeyMaps) {
		if (keys[km.dikCode] && !preKeys[km.dikCode]) {
			if (playerName.length() < nameLengthLimit) {
				playerName += km.character;
			}
		}
	}
}

void ScoreSystem::Add(int newScore) {
	// 最新の状態を読み直す
	scores_ = Load();

	// 名前を8文字にカット
	std::string limitedName = playerName.substr(0, 8);

	// 新しいデータを追加
	scores_.push_back({ limitedName, newScore });

	// スコア（RankingDataのscoreプロパティ）で大きい順に並び替え
	std::sort(scores_.begin(), scores_.end(), [](const RankingData& a, const RankingData& b) {
		return a.score > b.score;
		});

	// 追加したらすぐに保存
	Save();
}

void ScoreSystem::DrawScore(int score) {
	camera.MoveCameraTransform();

	// 背景描画
	Novice::DrawSprite(0, 0, BGtexture, 1.0f, 1.0f, 0.0f, WHITE);

	ImGui::Begin("Score Layout");
	ImGui::DragFloat2("ScorePos", &inputScorePos.x, 1.0f);
	ImGui::DragFloat2("NamePos", &inputNamePos.x, 1.0f);
	ImGui::DragFloat("Score Spacing", &inputScoreSpacing, 1.0f);
	ImGui::DragFloat("Name Spacing", &inputNameSpacing, 1.0f);
	ImGui::End();

	// スコア描画
	DrawScoreSprite(inputScorePos, score, inputScoreSpacing);
	DrawNameSprite(inputNamePos, playerName, inputNameSpacing);
}

void ScoreSystem::DrawRanking(char * keys,char * preKeys) {
	camera.MoveCameraTransform();

	if (keys[DIK_W]) {
		camera.MoveCamera({ 0.0f,5.0f });
	}

	if (keys[DIK_S]) {
		camera.MoveCamera({ 0.0f,-5.0f });
	}

	if (keys[DIK_R] && preKeys[DIK_R]) {
		camera.SetCameraPosition({ 0.0f,0.0f });
	}

	const std::vector<RankingData>& currentScores = GetScores();
	// --- ImGuiでの個別調整用UI ---
	ImGui::Begin("Ranking Detailed Layout");
	ImGui::DragFloat2("Rank Base Pos", &rankBasePos.x, 1.0f);
	ImGui::DragFloat("Rank Spacing", &rankSpacing, 1.0f);

	ImGui::DragFloat2("Name Base Pos", &nameBasePos.x, 1.0f);
	ImGui::DragFloat("Name Spacing", &nameSpacing, 1.0f);

	ImGui::DragFloat2("Score Base Pos", &scoreBasePos.x, 1.0f);
	ImGui::DragFloat("Score Spacing", &scoreSpacing, 1.0f);

	ImGui::DragFloat("Row Height", &rowHeight, 1.0f);
	ImGui::End();

	// --- 描画処理 ---
	for (int i = 0; i < static_cast<int>(currentScores.size()); i++) {
		float yOffset = i * -rowHeight;

		// 1. 順位 (Rank)
		Vector2 rPos = { rankBasePos.x, rankBasePos.y + yOffset };
		DrawScoreSprite(rPos, i + 1, rankSpacing);

		// 2. 名前 (Name)
		Vector2 nPos = { nameBasePos.x, rankBasePos.y + yOffset };
		std::string name = currentScores[i].name;
		DrawNameSprite(nPos, name, nameSpacing);

		// 3. スコア (Score)
		Vector2 sPos = { scoreBasePos.x, rankBasePos.y + yOffset };
		DrawScoreSprite(sPos, currentScores[i].score, scoreSpacing);
	}
	Novice::DrawSprite(0, 0, BGtexture, 1.0f, 1.0f, 0.0f, WHITE);
}//private-------------------------------------------------------------

std::vector<RankingData> ScoreSystem::Load() {
	std::vector<RankingData> loadScores;
	std::ifstream file(fileName);

	if (file.is_open()) {
		try {
			json j;
			file >> j;

			// ここが重要！JSONの配列から名前とスコアをセットで取り出す
			if (j.contains("scores") && j["scores"].is_array()) {
				for (auto& item : j["scores"]) {
					RankingData data;
					data.name = item.value("name", "Unknown");
					data.score = item.value("score", 0);
					loadScores.push_back(data);
				}
			}
		}
		catch (...) {
			// ファイルが壊れていた時の安全策
		}
		file.close();
	}

	return loadScores;
}

void ScoreSystem::Save() {
	json j_array = json::array();

	for (const auto& item : scores_) {
		// 名前とスコアをペアにして配列に入れる
		j_array.push_back({ {"name", item.name}, {"score", item.score} });
	}

	json j_final;
	j_final["scores"] = j_array;

	std::ofstream file(fileName);
	if (file.is_open()) {
		file << j_final.dump(4);
		file.close();
	}
}

const std::vector<RankingData>& ScoreSystem::GetScores() const {
	return scores_;
}

void ScoreSystem::Reset() {
	scores_.clear();
	Save();
}



void ScoreSystem::DrawScoreSprite(Vector2 pos, int score, float spacing) {
	std::string s = std::to_string(score);

	for (int i = 0; i < s.length(); i++) {
		int num = s[i] - '0';

		Vector2 pos_ = { pos.x + spacing * i,pos.y };

		Vector2 screenPos = camera.WorldToScreen(pos_);

		Novice::DrawSprite(
			static_cast<int>(screenPos.x),
			static_cast<int>(screenPos.y),
			numberTexture[num], 1.0f, 1.0f, 0.0f, WHITE);
	}
}

void ScoreSystem::DrawNameSprite(Vector2 pos, std::string& name, float spacing) {
	for (int i = 0; i < static_cast<int>(name.length()); i++) {
		char ch = name[i];
		int index = -1;
		int* textureArray = nullptr;

		// 文字種を判定してインデックスと配列を決定
		if (ch >= 'A' && ch <= 'Z') {
			index = ch - 'A';
			textureArray = upperAlphabetTexture;
		}
		else if (ch >= 'a' && ch <= 'z') {
			index = ch - 'a';
			textureArray = lowAlphabetTexture;
		}
		else if (ch >= '0' && ch <= '9') {
			index = ch - '0';
			textureArray = numberTexture;
		}

		// 有効な文字の場合のみ描画
		if (index != -1 && textureArray != nullptr) {

			Vector2 pos_ = { pos.x + spacing * i, pos.y };
			Vector2 screenPos = camera.WorldToScreen(pos_);

			Novice::DrawSprite(
				static_cast<int>(screenPos.x),
				static_cast<int>(screenPos.y),
				textureArray[index],
				1.0f,
				1.0f,
				0.0f,
				WHITE
			);
		}
	}
}
