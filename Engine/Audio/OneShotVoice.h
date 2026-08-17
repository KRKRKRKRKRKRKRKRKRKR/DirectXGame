#pragma once
#include <xaudio2.h>
#include <vector>

// 「鳴らして終わり」の使い捨てSourceVoiceを作る最小限のヘルパー。
// HitSoundComponentのプール管理（IsPlaying()のポーリングや経過時間の自前管理）が、
// XAudio2側の状態反映タイミングとズレて「まだ再生中の音を誤って打ち切る」不具合の
// 温床になっていたため、状態管理そのものをやめてこの方式に置き換えた。
//
// 仕組み：再生開始のたびに新しいSourceVoiceをCreateする。IXAudio2VoiceCallback::OnStreamEnd
// （バッファの再生が最後まで終わった通知）はXAudio2のオーディオ処理スレッドから呼ばれ、
// そこから直接DestroyVoiceを呼ぶことはXAudio2の仕様上禁止されている（コールバック内から
// ボイス操作すると内部でデッドロック/不正アクセスを起こす）。そのためコールバックは
// 「再生が終わった」フラグを立てるだけにし、実際のDestroyVoiceはUpdate()経由でメインスレッドから
// 行う。呼び出し側は同時に何個再生されているか・空きがあるかを一切気にしなくてよい
namespace OneShotVoice {

	// wfex/audioDataで指定した音声を1回だけ再生する。isBGMでBGM/SEサブミックスへ振り分ける。
	// 呼び出し側はaudioDataの生存期間を気にする必要はない（内部でコピーを保持し、
	// 再生完了時にUpdate()経由で自動的に解放する）
	void Play(const WAVEFORMATEX& wfex, const std::vector<BYTE>& audioData, float volume, bool isBGM);

	// 再生完了済みのSourceVoiceをまとめて破棄する。AudioManagerが毎フレーム（メインスレッドから）
	// 呼ぶ想定。OnStreamEndコールバック自体はオーディオ処理スレッドから呼ばれ、そこから
	// 直接DestroyVoiceできないため、実際の破棄処理をこの関数に一本化している
	void Update();

}
