#pragma once
#include <xaudio2.h>
#include <mfapi.h>
#include <wrl/client.h>
#include <string>
#include <vector>

enum class SoundType { BGM, SE };

class Sound; // 循環インクルードを避けるため前方宣言

// AudioManager に登録したサウンドの情報。DrawImGui で表示・操作する
struct SoundEntry {
    std::string name;
    Sound*      sound  = nullptr;
    SoundType   type   = SoundType::SE;
    bool        loop   = false;
    float       volume = 1.0f; // SourceVoice 個別の音量（0.0〜1.0）
};

class AudioManager {
public:
    static AudioManager& GetInstance();

    void Initialize();
    void Finalize();

    // ImGui で全オーディオ機能を表示する
    void DrawImGui();

    // ImGui で操作したいサウンドを登録・解除する
    void RegisterSound(const std::string& name, Sound* sound, SoundType type, bool loop = false);
    void UnregisterSound(Sound* sound);

    IXAudio2*            GetXAudio2()   const { return xAudio2_.Get(); }
    IXAudio2SubmixVoice* GetBGMSubmix() const { return bgmSubmix_; }
    IXAudio2SubmixVoice* GetSESubmix()  const { return seSubmix_; }

    void SetMasterVolume(float v);
    void SetBGMVolume(float v);
    void SetSEVolume(float v);

private:
    AudioManager() = default;
    ~AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // サウンドエントリのセクション（BGM / SE）を描画するヘルパー
    void DrawSoundSection(SoundType type);

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
    IXAudio2SubmixVoice*    bgmSubmix_   = nullptr;
    IXAudio2SubmixVoice*    seSubmix_    = nullptr;

    float masterVolume_ = 1.0f;
    float bgmVolume_    = 1.0f;
    float seVolume_     = 1.0f;
    bool  muteMaster_   = false;
    bool  muteBGM_      = false;
    bool  muteSE_       = false;

    std::vector<SoundEntry> sounds_;
};
