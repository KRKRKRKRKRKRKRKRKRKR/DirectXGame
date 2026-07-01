#include "InputDevice.h"
#include "../Utils/Logger.h" // InputDevice と Utils が同階層なのでそのまま

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
	keyboardDevice_->Acquire();
	keyboardDevice_->GetDeviceState(sizeof(key_), key_);
	memcpy(prevKey_, key_, sizeof(key_));

	mouseDevice_->Acquire();
	mouseDevice_->GetDeviceState(sizeof(mouseState_), &mouseState_);
}

void InputDevice::Finalize() {
	if (keyboardDevice_) {
		keyboardDevice_->Unacquire();
	}
	if (mouseDevice_) {
		mouseDevice_->Unacquire();
	}
}