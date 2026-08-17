#include "OneShotVoice.h"
#include "AudioManager.h"
#include <atomic>
#include <memory>

namespace OneShotVoice {

namespace {

// 1回分の再生を表すコールバックオブジェクト。XAudio2の仕様上、コールバック
// （OnStreamEnd、オーディオ処理スレッドから呼ばれる）の中からDestroyVoiceを呼んではいけない
// （呼ぶとデッドロック/不正アクセスを起こしうる）ため、ここでは「再生が終わった」フラグを
// 立てるだけにする。実際のDestroyVoice+破棄はUpdate()側（メインスレッド）で行う
class SelfDestructingVoice : public IXAudio2VoiceCallback {
public:
	IXAudio2SourceVoice* voice = nullptr;
	std::vector<BYTE> audioData; // SubmitSourceBuffer.pAudioDataが指す実データ。voice破棄まで保持する
	std::atomic<bool> finished{ false };

	void STDMETHODCALLTYPE OnStreamEnd() override { finished = true; }

	void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
	void STDMETHODCALLTYPE OnBufferStart(void*) override {}
	void STDMETHODCALLTYPE OnBufferEnd(void*) override {}
	void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
	void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}
};

// 再生中〜破棄待ちのボイス一覧。Update()（メインスレッド、毎フレーム）からのみ読み書きする
// 前提のため、Play()側からの追加もメインスレッドから行われる想定（GameObject::Update経由で
// HitSoundComponent::Play()が呼ばれるため、この前提は成立する）
std::vector<std::unique_ptr<SelfDestructingVoice>> g_activeVoices;

} // namespace

void Play(const WAVEFORMATEX& wfex, const std::vector<BYTE>& audioData, float volume, bool isBGM) {
	if (audioData.empty()) return;

	AudioManager& am = AudioManager::GetInstance();
	IXAudio2* xa2 = am.GetXAudio2();
	if (!xa2) return;

	IXAudio2SubmixVoice* submix = isBGM ? am.GetBGMSubmix() : am.GetSESubmix();
	XAUDIO2_SEND_DESCRIPTOR sendDesc = { 0, submix };
	XAUDIO2_VOICE_SENDS sends = { 1, &sendDesc };

	auto self = std::make_unique<SelfDestructingVoice>();
	self->audioData = audioData; // コピーを保持する（呼び出し元のキャッシュとは独立させる）

	HRESULT hr = xa2->CreateSourceVoice(&self->voice, &wfex, 0,
		XAUDIO2_DEFAULT_FREQ_RATIO, self.get(), &sends);
	if (FAILED(hr)) return; // selfはunique_ptrなのでここで自動的に破棄される

	self->voice->SetVolume(volume);

	XAUDIO2_BUFFER buf{};
	buf.pAudioData = self->audioData.data();
	buf.AudioBytes = static_cast<UINT32>(self->audioData.size());
	buf.Flags      = XAUDIO2_END_OF_STREAM;
	buf.LoopCount  = 0;

	self->voice->SubmitSourceBuffer(&buf);
	self->voice->Start();

	g_activeVoices.push_back(std::move(self));
}

void Update() {
	// 再生完了フラグが立ったものだけDestroyVoiceして一覧から取り除く
	// （メインスレッドからのみ呼ぶ前提のため、finishedの読み取り以外は同期不要）
	for (auto it = g_activeVoices.begin(); it != g_activeVoices.end();) {
		if ((*it)->finished.load()) {
			if ((*it)->voice) (*it)->voice->DestroyVoice();
			it = g_activeVoices.erase(it);
		} else {
			++it;
		}
	}
}

} // namespace OneShotVoice
