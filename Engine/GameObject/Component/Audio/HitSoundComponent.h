#pragma once
#include "../../IComponent.h"
#include "ProjectAssetEntry.h"
#include <string>
#include <vector>

// 攻撃がヒットした瞬間にSEを鳴らすコンポーネント。ReflexEnemyComponentと同じGameObjectに
// 付け、ReflexEnemyComponent::OnTriggerEnter（ダメージが入った瞬間）からPlay()を呼ぶ想定。
//
// 「鳴らして終わり」の一番シンプルな実装：Play()のたびにOneShotVoice::Playへ委譲するだけで、
// 同時発音数の制限やプール・再生中判定は一切持たない（XAudio2側が使い捨てのSourceVoiceを
// 再生完了時に自動破棄する）。以前は複数SourceVoiceをプールして使い回す最適化をしていたが、
// 「再生中かどうか」の判定（Sound::IsPlaying()のXAudio2状態ポーリング、その後試した経過時間の
// 自前管理）のタイミングが実際の発音とズレ、再生中の音を誤って打ち切る不具合が解消しなかった
// ため、状態管理そのものを撤廃した
//
// ファイル選択はTextureSelectorComponentと同じ「プロジェクトパネルの一覧からコンボで選ぶ」方式。
// audioClipsはSceneBase::projectAudioClips_（Resources/配下を走査した音声ファイル一覧）への
// 非所有ポインタで、パスを手入力させない。index=-1（audioClipsが空、または未選択）の間は
// 未設定状態として扱い、Play()は何もしない
class HitSoundComponent : public IComponent {
public:
	// audioClips: SceneBase::projectAudioClips_への非所有ポインタ（ComponentLoadContext::audioClips）。
	// initialIndex: audioClips内の初期選択インデックス（-1 = 未選択）
	HitSoundComponent(const std::vector<ProjectAssetEntry>* audioClips, int initialIndex = -1, float volume = 1.0f);

	// SEを1回再生する。未設定（index_<0）の場合は何もしない
	void Play();

	void DrawImGui(const char* namePrefix) override;

	// インデックスではなく名前（TextureSelectorComponentと同じ理由：audioClips_の並び順が
	// Resources/配下の走査結果に依存し、実行間で変わりうるため）を書き出す。復元は
	// ComponentRegistryのcreatorがこの名前から現在のindexを引き直す
	void ToJson(nlohmann::json& out) const override {
		out["audioName"] = (index_ >= 0 && index_ < static_cast<int>(audioClips_->size()))
			? (*audioClips_)[index_].displayName : std::string();
		out["volume"] = volume_;
	}

private:
	const std::vector<ProjectAssetEntry>* audioClips_;
	int index_;
	float volume_;
};
