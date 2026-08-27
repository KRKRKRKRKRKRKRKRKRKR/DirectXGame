#include "SpawnSoundComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../Audio/Sound.h"
#include "../../../Audio/OneShotVoice.h"
#include <string>
#include <unordered_map>

namespace {
// ファイルパス→デコード済みPCMデータのグローバルキャッシュ。HitSoundComponentと同じ理由
// （同じSEを何度も再生する際、毎回ファイル読み込み・MP3デコードをやり直すと重いため）。
// HitSoundComponent.cpp側のキャッシュとは別の翻訳単位内の無名namespaceで完結しており、
// 同じパスでも二重にデコードされる（Play/Spawnで別々のキャッシュを持つ）が、スポーンSEは
// 撃破SEと違って頻度が低いため実害はない
struct DecodedAudioCache {
	WAVEFORMATEX wfex{};
	std::vector<BYTE> audioData;
};
std::unordered_map<std::string, DecodedAudioCache> g_decodedAudioCache;

const DecodedAudioCache& GetOrDecode(const std::string& path) {
	auto it = g_decodedAudioCache.find(path);
	if (it == g_decodedAudioCache.end()) {
		Sound temp;
		temp.Load(path); // 初回のみ実ファイルを読み込み・デコードする
		DecodedAudioCache cache;
		cache.wfex = temp.GetFormat();
		cache.audioData = temp.GetAudioData();
		it = g_decodedAudioCache.emplace(path, std::move(cache)).first;
	}
	return it->second;
}
}

SpawnSoundComponent::SpawnSoundComponent(const std::vector<ProjectAssetEntry>* audioClips, int initialIndex, float volume)
	: audioClips_(audioClips), index_(initialIndex), volume_(volume) {
	if (index_ < 0 || index_ >= static_cast<int>(audioClips_->size())) index_ = -1;
}

void SpawnSoundComponent::Play() {
	if (index_ < 0) return; // 未設定状態
	const DecodedAudioCache& cache = GetOrDecode((*audioClips_)[index_].path);
	OneShotVoice::Play(cache.wfex, cache.audioData, volume_, /*isBGM=*/false);
}

void SpawnSoundComponent::DrawImGui(const char* namePrefix) {
	std::string comboLabel = std::string(namePrefix) + "SE";
	const char* currentName = (index_ >= 0) ? (*audioClips_)[index_].displayName.c_str() : "(未設定)";

	if (ImGui::BeginCombo(comboLabel.c_str(), currentName)) {
		for (int i = 0; i < static_cast<int>(audioClips_->size()); i++) {
			bool selected = (i == index_);
			if (ImGui::Selectable((*audioClips_)[i].displayName.c_str(), selected)) {
				index_ = i;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	std::string testLabel = std::string(namePrefix) + "テスト再生";
	if (index_ < 0) ImGui::BeginDisabled();
	if (ImGui::Button(testLabel.c_str())) {
		Play();
	}
	if (index_ < 0) ImGui::EndDisabled();

	std::string volLabel = std::string(namePrefix) + "音量";
	ImGui::SliderFloat(volLabel.c_str(), &volume_, 0.0f, 1.0f);
}
