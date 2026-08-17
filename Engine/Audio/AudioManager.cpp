#include "AudioManager.h"
#include "Sound.h"  // SoundEntry で Sound* の全定義が必要
#include "OneShotVoice.h"
#include "../../Externals/imgui/imgui.h"
#include <algorithm>
#include <cassert>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

AudioManager& AudioManager::GetInstance() {
    static AudioManager instance;
    return instance;
}

// ------------------------------------------------------------------ //
//  初期化 / 終了
// ------------------------------------------------------------------ //

void AudioManager::Initialize() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HRESULT hr = MFStartup(MF_VERSION);
    assert(SUCCEEDED(hr) && "MFStartup failed");

    hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr) && "XAudio2Create failed");

    hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(hr) && "CreateMasteringVoice failed");

    XAUDIO2_VOICE_DETAILS details;
    masterVoice_->GetVoiceDetails(&details);

    hr = xAudio2_->CreateSubmixVoice(&bgmSubmix_, details.InputChannels, details.InputSampleRate);
    assert(SUCCEEDED(hr) && "CreateSubmixVoice (BGM) failed");

    hr = xAudio2_->CreateSubmixVoice(&seSubmix_, details.InputChannels, details.InputSampleRate);
    assert(SUCCEEDED(hr) && "CreateSubmixVoice (SE) failed");
}

void AudioManager::Update() {
    OneShotVoice::Update();
}

void AudioManager::Finalize() {
    if (bgmSubmix_)  { bgmSubmix_->DestroyVoice();  bgmSubmix_  = nullptr; }
    if (seSubmix_)   { seSubmix_->DestroyVoice();   seSubmix_   = nullptr; }
    if (masterVoice_){ masterVoice_->DestroyVoice(); masterVoice_ = nullptr; }
    xAudio2_.Reset();
    MFShutdown();
    CoUninitialize();
}

// ------------------------------------------------------------------ //
//  サウンド登録
// ------------------------------------------------------------------ //

void AudioManager::RegisterSound(const std::string& name, Sound* sound, SoundType type, bool loop) {
    SoundEntry entry;
    entry.name  = name;
    entry.sound = sound;
    entry.type  = type;
    entry.loop  = loop;
    entry.volume = 1.0f;
    sounds_.push_back(entry);
}

void AudioManager::UnregisterSound(Sound* sound) {
    sounds_.erase(
        std::remove_if(sounds_.begin(), sounds_.end(),
            [sound](const SoundEntry& e) { return e.sound == sound; }),
        sounds_.end());
}

SoundEntry* AudioManager::FindEntry(Sound* sound) {
    for (auto& entry : sounds_) {
        if (entry.sound == sound) return &entry;
    }
    return nullptr;
}

// ------------------------------------------------------------------ //
//  音量 / ミュート
// ------------------------------------------------------------------ //

void AudioManager::SetMasterVolume(float v) {
    masterVolume_ = v;
    if (masterVoice_ && !muteMaster_) masterVoice_->SetVolume(v);
}

void AudioManager::SetBGMVolume(float v) {
    bgmVolume_ = v;
    if (bgmSubmix_ && !muteBGM_) bgmSubmix_->SetVolume(v);
}

void AudioManager::SetSEVolume(float v) {
    seVolume_ = v;
    if (seSubmix_ && !muteSE_) seSubmix_->SetVolume(v);
}

// ------------------------------------------------------------------ //
//  ImGui
// ------------------------------------------------------------------ //

// BGM / SE それぞれのサウンド一覧セクションを描画する
void AudioManager::DrawSoundSection(SoundType sectionType) {
    for (auto& entry : sounds_) {
        if (entry.type != sectionType) continue;

        ImGui::PushID(entry.name.c_str());
        ImGui::Separator();

        // 再生状態インジケーター
        bool playing = entry.sound->IsPlaying();
        bool paused  = entry.sound->IsPaused();
        if (paused) {
            ImGui::TextColored({ 1.0f, 0.8f, 0.2f, 1.0f }, "一時停止中");
        } else if (playing) {
            ImGui::TextColored({ 0.2f, 1.0f, 0.2f, 1.0f }, "再生中");
        } else {
            ImGui::TextColored({ 0.6f, 0.6f, 0.6f, 1.0f }, "停止中");
        }
        ImGui::SameLine();
        ImGui::Text("%s", entry.name.c_str());

        // Play / Pause(Resume) / Stop ボタン
        if (ImGui::Button("再生")) {
            entry.sound->Play(entry.loop, sectionType);
            entry.sound->SetVolume(entry.volume);
        }
        ImGui::SameLine();
        bool canPauseOrResume = paused || playing;
        if (!canPauseOrResume) ImGui::BeginDisabled();
        if (ImGui::Button(paused ? "再開" : "一時停止")) {
            if (paused) entry.sound->Resume(); else entry.sound->Pause();
        }
        if (!canPauseOrResume) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("停止")) {
            entry.sound->Stop();
        }
        ImGui::SameLine();
        ImGui::Checkbox("ループ", &entry.loop);

        // 個別音量（再生中のみ即時反映。停止中は次の Play 時に適用）
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::SliderFloat("音量##sound", &entry.volume, 0.0f, 1.0f)) {
            entry.sound->SetVolume(entry.volume);
        }

        ImGui::PopID();
    }
}

void AudioManager::DrawImGui() {
    ImGui::Begin("オーディオ##Audio");

    // ---- Master ----
    ImGui::Text("マスター");
    if (ImGui::Button("再生##master")) {
        if (xAudio2_) xAudio2_->StartEngine();
        masterPlaying_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("停止##master")) {
        if (xAudio2_) xAudio2_->StopEngine();
        masterPlaying_ = false;
    }

    ImGui::SameLine();
    ImGui::TextColored(
        masterPlaying_ ? ImVec4{ 0.2f, 1.0f, 0.2f, 1.0f } : ImVec4{ 0.6f, 0.6f, 0.6f, 1.0f },
        masterPlaying_ ? "再生中" : "停止中");
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::SliderFloat("##master", &masterVolume_, 0.0f, 1.0f)) {
        if (!muteMaster_ && masterVoice_) masterVoice_->SetVolume(masterVolume_);
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("ミュート##master", &muteMaster_)) {
        if (masterVoice_) masterVoice_->SetVolume(muteMaster_ ? 0.0f : masterVolume_);
    }

    ImGui::Spacing();

    // ---- BGM ----
    if (ImGui::CollapsingHeader("BGM", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("音量##bgm", &bgmVolume_, 0.0f, 1.0f)) {
            if (!muteBGM_ && bgmSubmix_) bgmSubmix_->SetVolume(bgmVolume_);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("ミュート##bgm", &muteBGM_)) {
            if (bgmSubmix_) bgmSubmix_->SetVolume(muteBGM_ ? 0.0f : bgmVolume_);
        }
        DrawSoundSection(SoundType::BGM);
    }

    ImGui::Spacing();

    // ---- SE ----
    if (ImGui::CollapsingHeader("SE", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("音量##se", &seVolume_, 0.0f, 1.0f)) {
            if (!muteSE_ && seSubmix_) seSubmix_->SetVolume(seVolume_);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("ミュート##se", &muteSE_)) {
            if (seSubmix_) seSubmix_->SetVolume(muteSE_ ? 0.0f : seVolume_);
        }
        DrawSoundSection(SoundType::SE);
    }

    ImGui::End();
}
