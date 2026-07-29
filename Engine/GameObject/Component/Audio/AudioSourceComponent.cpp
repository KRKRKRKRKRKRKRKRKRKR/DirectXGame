#include "AudioSourceComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

AudioSourceComponent::AudioSourceComponent(const std::string& filePath, const std::string& registeredName,
	SoundType type, bool loop, bool playOnAwake, float volume)
	: filePath_(filePath), registeredName_(registeredName), type_(type), loop_(loop),
	  playOnAwake_(playOnAwake), volume_(volume) {
	sound_.Load(filePath);
	AudioManager::GetInstance().RegisterSound(registeredName, &sound_, type, loop);
	if (SoundEntry* entry = AudioManager::GetInstance().FindEntry(&sound_)) {
		entry->volume = volume_; // RegisterSoundは既定で1.0fにするため、保存済みの音量で上書きする
	}
	if (playOnAwake_) {
		sound_.Play(loop_, type_);
		sound_.SetVolume(volume_);
	}
}

AudioSourceComponent::~AudioSourceComponent() {
	AudioManager::GetInstance().UnregisterSound(&sound_);
}

void AudioSourceComponent::DrawImGui(const char* namePrefix) {
	std::string info = std::string(namePrefix) + registeredName_ + " (" + (type_ == SoundType::BGM ? "BGM" : "SE") + ")";
	ImGui::Text("%s", info.c_str());

	bool playing = sound_.IsPlaying();
	bool paused  = sound_.IsPaused();
	const char* statusText  = paused ? "一時停止中" : (playing ? "再生中" : "停止中");
	ImVec4      statusColor = paused ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f)
		: (playing ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
	ImGui::TextColored(statusColor, "%s", statusText);

	std::string playLabel  = std::string(namePrefix) + "再生";
	std::string pauseLabel = std::string(namePrefix) + (paused ? "再開" : "一時停止");
	std::string stopLabel  = std::string(namePrefix) + "停止";
	if (ImGui::Button(playLabel.c_str())) {
		sound_.Play(loop_, type_);
		sound_.SetVolume(volume_);
	}
	ImGui::SameLine();
	// 一時停止中はボタンを「再開」に切り替える。再生していない（Stop済み）ときに一時停止しても
	// 何も起きないため無効化する（Pause()自体もsourceVoice_が無ければ何もしないが、
	// ボタンの見た目でも「押しても意味が無い」ことを伝える）
	bool canPauseOrResume = paused || playing;
	if (!canPauseOrResume) ImGui::BeginDisabled();
	if (ImGui::Button(pauseLabel.c_str())) {
		if (paused) sound_.Resume(); else sound_.Pause();
	}
	if (!canPauseOrResume) ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button(stopLabel.c_str())) {
		sound_.Stop();
	}

	// loop/volumeはAudioManager側のSoundEntry（グローバル「オーディオ」パネルと同じ実体）にも
	// 書き戻し、両方の表示が常に一致するようにする
	std::string loopLabel = std::string(namePrefix) + "ループ";
	if (ImGui::Checkbox(loopLabel.c_str(), &loop_)) {
		if (SoundEntry* entry = AudioManager::GetInstance().FindEntry(&sound_)) entry->loop = loop_;
	}

	std::string awakeLabel = std::string(namePrefix) + "開始時に自動再生 (Play On Awake)";
	ImGui::Checkbox(awakeLabel.c_str(), &playOnAwake_);

	std::string volLabel = std::string(namePrefix) + "音量";
	if (ImGui::SliderFloat(volLabel.c_str(), &volume_, 0.0f, 1.0f)) {
		sound_.SetVolume(volume_);
		if (SoundEntry* entry = AudioManager::GetInstance().FindEntry(&sound_)) entry->volume = volume_;
	}
}
