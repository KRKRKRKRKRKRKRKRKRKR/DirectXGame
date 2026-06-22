#include "DeltaTime.h"

void DeltaTime::Start() {
	QueryPerformanceFrequency(&frequency_);
	QueryPerformanceCounter(&lastTime_);
}

void DeltaTime::Update() {
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	deltaTime_ = static_cast<float>(currentTime.QuadPart - lastTime_.QuadPart) / static_cast<float>(frequency_.QuadPart);
	lastTime_ = currentTime;
}

float DeltaTime::GetDeltaTime() const {
	return deltaTime_;
}