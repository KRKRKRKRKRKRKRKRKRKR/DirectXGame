#pragma once
#include "AudioManager.h"
#include <string>
#include <vector>

class Sound {
public:
    Sound() = default;
    ~Sound() { Unload(); }

    // WAVファイルを読み込む。Load後にPlayを呼ぶ
    void Load(const std::string& filePath);

    // 既にデコード済みのPCMデータ（wfex/audioData）をコピーして使う。同じ音声ファイルを
    // 複数のSoundインスタンスで鳴らしたい場合（HitSoundComponentの再生プール等）、
    // Load()で毎回ファイル読み込み・デコードをやり直すと重いため、呼び出し側が一度だけ
    // Load()したSoundのGetDecodedData()結果をここに渡して使い回す
    void LoadFromDecoded(const WAVEFORMATEX& wfex, const std::vector<BYTE>& audioData);

    // 他のSoundへLoadFromDecodedで渡すための、デコード済みPCMデータへの参照。
    // Load()を呼んだ後でないと空/未初期化の状態のまま
    const WAVEFORMATEX& GetFormat() const { return wfex_; }
    const std::vector<BYTE>& GetAudioData() const { return audioData_; }

    // 再生。type で BGM / SE サブミックスに振り分ける
    void Play(bool loop = false, SoundType type = SoundType::SE);

    // 再生を止めてSourceVoiceを破棄する（再開すると最初から再生し直しになる）
    void Stop();

    // 再生位置を保持したまま一時停止する（Stop()と違いバッファをFlushせずSourceVoiceも破棄しない）。
    // XAudio2のIXAudio2SourceVoice::Stop()はFlushSourceBuffersを呼ばない限り
    // キュー済みバッファの消費位置を保持するため、これを利用してPause/Resumeを実現している
    void Pause();

    // Pause()の続きから再生を再開する
    void Resume();

    // 音量を変更（0.0〜1.0）
    void SetVolume(float volume);

    // 再生中かどうか（一時停止中はfalseを返す）
    bool IsPlaying() const;

    // 一時停止中かどうか
    bool IsPaused() const { return paused_; }

    // 音声データとSourceVoiceを解放
    void Unload();

private:
    void LoadWav(const std::string& filePath);
    void LoadMp3(const std::string& filePath);  // Media Foundation 経由（mp3/aac/wma など）

    WAVEFORMATEX wfex_{};
    std::vector<BYTE> audioData_;
    IXAudio2SourceVoice* sourceVoice_ = nullptr;
    bool paused_ = false;
};
