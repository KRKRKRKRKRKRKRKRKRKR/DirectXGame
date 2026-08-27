#pragma once
#include "../../IComponent.h"
#include "ProjectAssetEntry.h"
#include <string>
#include <vector>

// 敵がスポーンした瞬間にSEを鳴らすコンポーネント。HitSoundComponentと完全に同じ構造
// （テンプレートに付け、PlayScene::BuildEnemyFromTemplateDataが読み取って複製先の敵にも
// 付与し、スポーン直後にPlay()を呼ぶ想定）。
//
// 「鳴らして終わり」の一番シンプルな実装：Play()のたびにOneShotVoice::Playへ委譲するだけで、
// 同時発音数の制限やプール・再生中判定は一切持たない（詳しくはHitSoundComponentのコメント参照）。
//
// ファイル選択はTextureSelectorComponentと同じ「プロジェクトパネルの一覧からコンボで選ぶ」方式。
// audioClipsはSceneBase::projectAudioClips_（Resources/配下を走査した音声ファイル一覧）への
// 非所有ポインタで、パスを手入力させない。index=-1（audioClipsが空、または未選択）の間は
// 未設定状態として扱い、Play()は何もしない
class SpawnSoundComponent : public IComponent {
public:
	// audioClips: SceneBase::projectAudioClips_への非所有ポインタ（ComponentLoadContext::audioClips）。
	// initialIndex: audioClips内の初期選択インデックス（-1 = 未選択）
	SpawnSoundComponent(const std::vector<ProjectAssetEntry>* audioClips, int initialIndex = -1, float volume = 1.0f);

	// SEを1回再生する。未設定（index_<0）の場合は何もしない
	void Play();

	void DrawImGui(const char* namePrefix) override;

	// インデックスではなく名前（HitSoundComponentと同じ理由：audioClips_の並び順が
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
