#pragma once
#include "../../IComponent.h"
#include "../../../Audio/Sound.h"
#include "../../../Audio/AudioManager.h"
#include <string>

// GameObjectに音源を持たせるコンポーネント。コンストラクタでSoundをLoadし、AudioManagerへ
// 登録する。UnityのAudioSourceを参考に、Play On Awake（生成時に自動再生するか）・Loop・
// Volumeと、Inspector上のPlay/Stopボタンをこのコンポーネント自身に持たせている
// （グローバルな「オーディオ」パネル＝AudioManager::DrawImGui()と同じSoundEntryを
// AudioManager::FindEntryで直接読み書きするため、二重に状態を持たず表示がズレない）。
// BGM等の非空間音源は3D位置を持たないため、付与先のGameObjectのTransformは使わない
// （3D距離減衰は現状のXAudio2セットアップが対応していないため未対応）
class AudioSourceComponent : public IComponent {
public:
	AudioSourceComponent(const std::string& filePath, const std::string& registeredName,
		SoundType type = SoundType::BGM, bool loop = true, bool playOnAwake = true, float volume = 1.0f);
	~AudioSourceComponent() override;

	Sound& GetSound() { return sound_; }

	void DrawImGui(const char* namePrefix) override;

	// コンストラクタ引数一式をJSONへ書き出す（復元はコンストラクタを呼び直す形になるため、
	// FromJsonでは何もしない。ComponentRegistryのcreatorがこれらの値を読んでAddComponentし直す）
	void ToJson(nlohmann::json& out) const override {
		out["filePath"] = filePath_;
		out["registeredName"] = registeredName_;
		out["soundType"] = static_cast<int>(type_);
		out["loop"] = loop_;
		out["playOnAwake"] = playOnAwake_;
		out["volume"] = volume_;
	}

private:
	Sound sound_;
	std::string filePath_;
	std::string registeredName_;
	SoundType type_;
	bool loop_;
	bool playOnAwake_;
	float volume_;
};
