#include "InputDevice.h"
#include "../Utils/Logger.h" // InputDevice と Utils が同階層なのでそのまま

void InputDevice::Initialize(HINSTANCE hInstance, HWND hwnd) {
	HRESULT hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput_, nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed DirectInput8Create\n");
		return;
	}
	hr = directInput_->CreateDevice(GUID_SysKeyboard, &keyboardDevice_, nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDevice for keyboard\n");
		directInput_->Release();
		return;
	}
	hr = keyboardDevice_->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr)) {
		Logger::Log("Failed SetDataFormat for keyboard\n");
		keyboardDevice_->Release();
		directInput_->Release();
		return;
	}
	hr = keyboardDevice_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr)) {
		Logger::Log("Failed SetCooperativeLevel for keyboard\n");
		keyboardDevice_->Release();
		directInput_->Release();
		return;
	}

	hr = directInput_->CreateDevice(GUID_SysMouse, &mouseDevice_, nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDevice for mouse\n");
		keyboardDevice_->Release();
		directInput_->Release();
		return;
	}

	hr = mouseDevice_->SetDataFormat(&c_dfDIMouse);
	if (FAILED(hr)) {
		Logger::Log("Failed SetDataFormat for mouse\n");
		mouseDevice_->Release();
		keyboardDevice_->Release();
		directInput_->Release();
		return;
	}

	hr = mouseDevice_->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(hr)) {
		Logger::Log("Failed SetCooperativeLevel for mouse\n");
		mouseDevice_->Release();
		keyboardDevice_->Release();
		directInput_->Release();
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
		keyboardDevice_->Release();
		keyboardDevice_ = nullptr;
	}
	if (mouseDevice_) {
		mouseDevice_->Unacquire();
		mouseDevice_->Release();
		mouseDevice_ = nullptr;
	}
	if (directInput_) {
		directInput_->Release();
		directInput_ = nullptr;
	}
}