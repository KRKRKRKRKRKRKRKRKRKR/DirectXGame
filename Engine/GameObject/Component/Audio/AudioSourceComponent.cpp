#include "AudioSourceComponent.h"

AudioSourceComponent::AudioSourceComponent(const std::string& filePath, const std::string& registeredName,
	SoundType type, bool loop)
	: filePath_(filePath), registeredName_(registeredName), type_(type), loop_(loop) {
	sound_.Load(filePath);
	AudioManager::GetInstance().RegisterSound(registeredName, &sound_, type, loop);
}

AudioSourceComponent::~AudioSourceComponent() {
	AudioManager::GetInstance().UnregisterSound(&sound_);
}
