#pragma once
#include <Windows.h>
class DeltaTime
{
public:
	void Start();
	void Update();
	float GetDeltaTime() const;
private:
	LARGE_INTEGER frequency_;// 周波数
	LARGE_INTEGER lastTime_;// 前回の時間
	float deltaTime_ = 0.0f;// 前回のフレームからの経過時間
};

