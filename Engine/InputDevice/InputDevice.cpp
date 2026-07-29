#include "InputDevice.h"
#include "../Utils/Logger.h" // InputDevice と Utils が同階層なのでそのまま
#include <algorithm>
#include <cmath>

namespace {
// スティック入力(-32768〜32767の生値2軸)を、Microsoft推奨の円形デッドゾーンで
// -1.0〜1.0のfloatへ正規化する。軸ごとに単純クランプすると斜め入力だけデッドゾーンが
// 狭く/角ばって感じられるため、距離（半径）で判定してから向きを保ったまま正規化する
void ApplyCircularDeadzone(SHORT rawX, SHORT rawY, float deadzone, float& outX, float& outY) {
    float x = static_cast<float>(rawX);
    float y = static_cast<float>(rawY);
    float magnitude = std::sqrt(x * x + y * y);

    if (magnitude < deadzone) {
        outX = 0.0f;
        outY = 0.0f;
        return;
    }

    // デッドゾーンの外側だけを0〜1へ再マッピングする（デッドゾーン境界でいきなり全力入力に
    // 飛ばないようにする）
    constexpr float kMaxStickValue = 32767.0f;
    float normalizedMagnitude = (std::min)((magnitude - deadzone) / (kMaxStickValue - deadzone), 1.0f);
    outX = (x / magnitude) * normalizedMagnitude;
    outY = (y / magnitude) * normalizedMagnitude;
}
}

void InputDevice::Initialize(HINSTANCE hInstance, HWND hwnd) {
	HRESULT hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
		reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed DirectInput8Create\n");
		return;
	}
	hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboardDevice_.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDevice for keyboard\n");
		return;
	}
	hr = keyboardDevice_->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr)) {
		Logger::Log("Failed SetDataFormat for keyboard\n");
		return;
	}
	hr = keyboardDevice_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr)) {
		Logger::Log("Failed SetCooperativeLevel for keyboard\n");
		return;
	}

	hr = directInput_->CreateDevice(GUID_SysMouse, mouseDevice_.GetAddressOf(), nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDevice for mouse\n");
		return;
	}
	hr = mouseDevice_->SetDataFormat(&c_dfDIMouse);
	if (FAILED(hr)) {
		Logger::Log("Failed SetDataFormat for mouse\n");
		return;
	}
	hr = mouseDevice_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr)) {
		Logger::Log("Failed SetCooperativeLevel for mouse\n");
		return;
	}
}

void InputDevice::Update() {
	memcpy(prevKey_, key_, sizeof(key_)); // 新しい状態を取得する前に、直前フレームの状態を退避する
	keyboardDevice_->Acquire();
	keyboardDevice_->GetDeviceState(sizeof(key_), key_);

	mouseDevice_->Acquire();
	mouseDevice_->GetDeviceState(sizeof(mouseState_), &mouseState_);

	UpdateGamepad();
}

void InputDevice::UpdateGamepad() {
	// XInputはDirectInputと違いAcquire等の事前登録が不要で、XInputGetStateを毎回呼ぶだけでよい。
	// 未接続のコントローラ番号はERROR_DEVICE_NOT_CONNECTEDを返すだけなのでポーリングし続けて安全
	for (int i = 0; i < kGamepadCount; ++i) {
		prevGamepadState_[i] = gamepadState_[i]; // 前フレームの状態をトリガー判定用に退避する

		XINPUT_STATE state{};
		DWORD result = XInputGetState(static_cast<DWORD>(i), &state);
		gamepadConnected_[i] = (result == ERROR_SUCCESS);
		gamepadState_[i] = gamepadConnected_[i] ? state : XINPUT_STATE{};
	}
}

void InputDevice::Finalize() {
	if (keyboardDevice_) {
		keyboardDevice_->Unacquire();
	}
	if (mouseDevice_) {
		mouseDevice_->Unacquire();
	}
}

// ---- ゲームパッド（XInput） ----

bool InputDevice::IsGamepadConnectedInternal(int index) const {
	if (index < 0 || index >= kGamepadCount) return false;
	return gamepadConnected_[index];
}

bool InputDevice::IsGamepadButtonPressedInternal(int index, WORD button) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return false;
	return (gamepadState_[index].Gamepad.wButtons & button) != 0;
}

bool InputDevice::IsGamepadButtonTriggeredInternal(int index, WORD button) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return false;
	bool now  = (gamepadState_[index].Gamepad.wButtons & button) != 0;
	bool prev = (prevGamepadState_[index].Gamepad.wButtons & button) != 0;
	return now && !prev;
}

float InputDevice::GetGamepadLeftStickXInternal(int index) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return 0.0f;
	float x, y;
	ApplyCircularDeadzone(gamepadState_[index].Gamepad.sThumbLX, gamepadState_[index].Gamepad.sThumbLY,
		XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, x, y);
	return x;
}

float InputDevice::GetGamepadLeftStickYInternal(int index) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return 0.0f;
	float x, y;
	ApplyCircularDeadzone(gamepadState_[index].Gamepad.sThumbLX, gamepadState_[index].Gamepad.sThumbLY,
		XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, x, y);
	return y;
}

float InputDevice::GetGamepadRightStickXInternal(int index) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return 0.0f;
	float x, y;
	ApplyCircularDeadzone(gamepadState_[index].Gamepad.sThumbRX, gamepadState_[index].Gamepad.sThumbRY,
		XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, x, y);
	return x;
}

float InputDevice::GetGamepadRightStickYInternal(int index) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return 0.0f;
	float x, y;
	ApplyCircularDeadzone(gamepadState_[index].Gamepad.sThumbRX, gamepadState_[index].Gamepad.sThumbRY,
		XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, x, y);
	return y;
}

float InputDevice::GetGamepadLeftTriggerInternal(int index) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return 0.0f;
	BYTE raw = gamepadState_[index].Gamepad.bLeftTrigger;
	if (raw < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return 0.0f;
	return static_cast<float>(raw) / 255.0f;
}

float InputDevice::GetGamepadRightTriggerInternal(int index) const {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return 0.0f;
	BYTE raw = gamepadState_[index].Gamepad.bRightTrigger;
	if (raw < XINPUT_GAMEPAD_TRIGGER_THRESHOLD) return 0.0f;
	return static_cast<float>(raw) / 255.0f;
}

void InputDevice::SetGamepadVibrationInternal(int index, float leftMotor, float rightMotor) {
	if (index < 0 || index >= kGamepadCount || !gamepadConnected_[index]) return;
	XINPUT_VIBRATION vibration{};
	vibration.wLeftMotorSpeed  = static_cast<WORD>(std::clamp(leftMotor, 0.0f, 1.0f) * 65535.0f);
	vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(rightMotor, 0.0f, 1.0f) * 65535.0f);
	XInputSetState(static_cast<DWORD>(index), &vibration);
}