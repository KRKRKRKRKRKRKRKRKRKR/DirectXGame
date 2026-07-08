#pragma once
#include "../../IComponent.h"
#include "../../../Audio/Sound.h"
#include "../../../Audio/AudioManager.h"
#include <string>

// GameObjectに音源を持たせるコンポーネント。コンストラクタでSoundをLoadし、AudioManagerへ
// 登録する（自動再生はしない。再生自体はAudioManager::DrawImGui()のパネルから行う運用）。
// BGM等の非空間音源は3D位置を持たないため、付与先のGameObjectのTransformは使わない
class AudioSourceComponent : public IComponent {
public:
	AudioSourceComponent(const std::string& filePath, const std::string& registeredName,
		SoundType type = SoundType::BGM, bool loop = true);
	~AudioSourceComponent() override;

	Sound& GetSound() { return sound_; }

	// コンストラクタ引数一式をJSONへ書き出す（復元はコンストラクタを呼び直す形になるため、
	// FromJsonでは何もしない。ComponentRegistryのcreatorがこれらの値を読んでAddComponentし直す）
	void ToJson(nlohmann::json& out) const override {
		out["filePath"] = filePath_;
		out["registeredName"] = registeredName_;
		out["soundType"] = static_cast<int>(type_);
		out["loop"] = loop_;
	}

private:
	Sound sound_;
	std::string filePath_;
	std::string registeredName_;
	SoundType type_;
	bool loop_;
};
