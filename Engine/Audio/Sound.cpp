#include "Sound.h"
#include "AudioManager.h"
#include "../Utils/StringUtils.h"
#include <mfidl.h>
#include <mfreadwrite.h>
#include <fstream>
#include <algorithm>
#include <cassert>

#pragma comment(lib, "mfreadwrite.lib")

// ---- WAV ローダー --------------------------------------------------------

struct ChunkHeader {
    char     id[4];
    uint32_t size;
};

void Sound::LoadWav(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    assert(file.is_open() && "WAV file not found");

    ChunkHeader riff;
    file.read(reinterpret_cast<char*>(&riff), sizeof(ChunkHeader));
    assert(std::string(riff.id, 4) == "RIFF" && "Not a RIFF file");

    char wave[4];
    file.read(wave, 4);
    assert(std::string(wave, 4) == "WAVE" && "Not a WAVE file");

    bool fmtFound = false, dataFound = false;
    while (!fmtFound || !dataFound) {
        ChunkHeader chunk;
        if (!file.read(reinterpret_cast<char*>(&chunk), sizeof(ChunkHeader))) break;

        if (std::string(chunk.id, 4) == "fmt ") {
            file.read(reinterpret_cast<char*>(&wfex_), sizeof(WAVEFORMATEX));
            if (chunk.size > sizeof(WAVEFORMATEX))
                file.seekg(chunk.size - sizeof(WAVEFORMATEX), std::ios::cur);
            fmtFound = true;
        } else if (std::string(chunk.id, 4) == "data") {
            audioData_.resize(chunk.size);
            file.read(reinterpret_cast<char*>(audioData_.data()), chunk.size);
            dataFound = true;
        } else {
            file.seekg(chunk.size, std::ios::cur);
        }
    }

    assert(fmtFound  && "fmt chunk not found");
    assert(dataFound && "data chunk not found");
}

// ---- MP3 / AAC / WMA ローダー（Media Foundation） ------------------------
//
// 仕組み：
//   IMFSourceReader がファイルを読んでデコードする
//   出力フォーマットを PCM に指定 → どんな圧縮形式でも同じコードで扱える
//   全フレームを audioData_ に溜め込んだら XAudio2 に渡す

void Sound::LoadMp3(const std::string& filePath) {
    std::wstring wPath = StringUtils::ConvertString(filePath);

    // SourceReader を作成（ファイルを開いてデコーダーをセット）
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(wPath.c_str(), nullptr, &reader);
    assert(SUCCEEDED(hr) && "MFCreateSourceReaderFromURL failed — ファイルパスを確認してください");

    // 第一オーディオストリームだけ有効にする
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);

    // 出力フォーマットを「非圧縮 PCM」に指定
    Microsoft::WRL::ComPtr<IMFMediaType> pcmType;
    MFCreateMediaType(&pcmType);
    pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pcmType->SetGUID(MF_MT_SUBTYPE,    MFAudioFormat_PCM);
    hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get());
    assert(SUCCEEDED(hr) && "SetCurrentMediaType failed");

    // デコード後の実際のフォーマットを取得して WAVEFORMATEX を埋める
    Microsoft::WRL::ComPtr<IMFMediaType> outType;
    reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outType);

    WAVEFORMATEX* pWfx = nullptr;
    UINT32 cbFormat = 0;
    MFCreateWaveFormatExFromMFMediaType(outType.Get(), &pWfx, &cbFormat);
    memcpy(&wfex_, pWfx, sizeof(WAVEFORMATEX));
    CoTaskMemFree(pWfx);

    // 全サンプルを読み込んで audioData_ に追記
    while (true) {
        DWORD flags = 0;
        Microsoft::WRL::ComPtr<IMFSample> sample;
        hr = reader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, nullptr, &flags, nullptr, &sample);

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
        if (!sample) continue;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
        sample->ConvertToContiguousBuffer(&buffer);

        BYTE*  pData = nullptr;
        DWORD  cbLen = 0;
        buffer->Lock(&pData, nullptr, &cbLen);

        size_t offset = audioData_.size();
        audioData_.resize(offset + cbLen);
        memcpy(audioData_.data() + offset, pData, cbLen);

        buffer->Unlock();
    }

    assert(!audioData_.empty() && "デコード後のオーディオデータが空です");
}

// ---- 公開 API ------------------------------------------------------------

void Sound::Load(const std::string& filePath) {
    // 拡張子を小文字で取得して振り分け
    size_t dot = filePath.rfind('.');
    std::string ext = (dot != std::string::npos) ? filePath.substr(dot) : "";
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".wav") {
        LoadWav(filePath);
    } else {
        // .mp3 / .aac / .wma など Media Foundation が対応する形式はすべて受け付ける
        LoadMp3(filePath);
    }
}

void Sound::Play(bool loop, SoundType type) {
    assert(!audioData_.empty() && "Sound::Load を先に呼んでください");

    Stop();
    paused_ = false; // 新規再生なので一時停止状態はクリアする

    AudioManager& am = AudioManager::GetInstance();
    IXAudio2* xa2 = am.GetXAudio2();
    if (!xa2) return;

    // BGM か SE かに応じてサブミックスボイスへルーティング
    IXAudio2SubmixVoice* submix = (type == SoundType::BGM)
        ? am.GetBGMSubmix()
        : am.GetSESubmix();

    XAUDIO2_SEND_DESCRIPTOR sendDesc = { 0, submix };
    XAUDIO2_VOICE_SENDS sends = { 1, &sendDesc };

    HRESULT hr = xa2->CreateSourceVoice(&sourceVoice_, &wfex_, 0,
        XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, &sends);
    assert(SUCCEEDED(hr) && "CreateSourceVoice failed");

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = audioData_.data();
    buf.AudioBytes = static_cast<UINT32>(audioData_.size());
    buf.Flags      = XAUDIO2_END_OF_STREAM;
    buf.LoopCount  = loop ? XAUDIO2_LOOP_INFINITE : 0;

    sourceVoice_->SubmitSourceBuffer(&buf);
    sourceVoice_->Start();
}

void Sound::Stop() {
    paused_ = false;
    if (!sourceVoice_) return;

    // AudioManager::Finalize() 後は XAudio2 が null になる。
    // その状態で SourceVoice を操作するとクラッシュするのでポインタだけ捨てる
    if (!AudioManager::GetInstance().GetXAudio2()) {
        sourceVoice_ = nullptr;
        return;
    }

    sourceVoice_->Stop();
    sourceVoice_->FlushSourceBuffers();
    sourceVoice_->DestroyVoice();
    sourceVoice_ = nullptr;
}

void Sound::Pause() {
    if (!sourceVoice_ || paused_) return;
    // FlushSourceBuffersを呼ばないことがStop()（完全停止）との違い。キュー済みバッファの
    // 消費位置はSourceVoice内部に残るため、Start()で一時停止した位置から再開できる
    sourceVoice_->Stop();
    paused_ = true;
}

void Sound::Resume() {
    if (!sourceVoice_ || !paused_) return;
    sourceVoice_->Start();
    paused_ = false;
}

void Sound::SetVolume(float volume) {
    if (sourceVoice_) {
        sourceVoice_->SetVolume(volume);
    }
}

bool Sound::IsPlaying() const {
    if (!sourceVoice_ || paused_) return false;
    XAUDIO2_VOICE_STATE state;
    sourceVoice_->GetState(&state);
    return state.BuffersQueued > 0;
}

void Sound::Unload() {
    Stop();
    audioData_.clear();
    audioData_.shrink_to_fit();
}
