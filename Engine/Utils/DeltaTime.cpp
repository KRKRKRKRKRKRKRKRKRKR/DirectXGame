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

	// 起動直後のアセット読み込み（Start()はGame::Initialize()より前に呼ばれるため、
	// テクスチャ/モデル読み込み時間がまるごと最初のdeltaTimeに乗ってしまう）や、
	// ブレークポイント等で1フレームの間隔が異常に空いた場合に、重力や移動等
	// deltaTimeを使う全ての処理が一気に吹き飛ぶのを防ぐため上限をクランプする
	constexpr float kMaxDeltaTime = 1.0f / 15.0f; // 最低15fps相当までを許容
	if (deltaTime_ > kMaxDeltaTime) deltaTime_ = kMaxDeltaTime;
}

float DeltaTime::GetDeltaTime() const {
	return deltaTime_;
}