#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class InputDevice {
public:
    static InputDevice& GetInstance() {
        static InputDevice instance;
        return instance;
    }

    InputDevice(const InputDevice&) = delete;
    InputDevice& operator=(const InputDevice&) = delete;

    void Initialize(HINSTANCE hInstance, HWND hwnd);
    void Update();
    void Finalize();

    // 内部用の判定関数
    bool IsPressedInternal(BYTE key) const { return (key_[key] & 0x80) != 0; }
    bool IsTriggeredInternal(BYTE key) const { return ((key_[key] & 0x80) && !(prevKey_[key] & 0x80)); }

    long GetMouseX() const { return mouseState_.lX; }
    long GetMouseY() const { return mouseState_.lY; }
    bool GetMouseLeftPressed() const { return (mouseState_.rgbButtons[0] & 0x80) != 0; }
	bool GetMouseRightPressed() const { return (mouseState_.rgbButtons[1] & 0x80) != 0; }
    long GetMouseWheel() const { return mouseState_.lZ; }

private:
    InputDevice() = default;
    ~InputDevice() = default;

    //キーボード
    IDirectInput8* directInput_ = nullptr;
    IDirectInputDevice8* keyboardDevice_ = nullptr;
    BYTE key_[256] = {};
    BYTE prevKey_[256] = {};

    //マウス
    IDirectInputDevice8* mouseDevice_ = nullptr;
    DIMOUSESTATE mouseState_ = {};
};

// --- ここでインスタンスの呼び出しを隠蔽する ---
namespace Input {
    inline bool IsPressed(BYTE key) {
        return InputDevice::GetInstance().IsPressedInternal(key);
    }

    inline bool IsTriggered(BYTE key) {
        return InputDevice::GetInstance().IsTriggeredInternal(key);
    }

    inline long GetMouseMoveX() {
        return InputDevice::GetInstance().GetMouseX();
    }

    inline long GetMouseMoveY() {
        return InputDevice::GetInstance().GetMouseY();
    }

    inline bool IsMouseLeftPressed() {
        return InputDevice::GetInstance().GetMouseLeftPressed();
    }

    inline bool IsMouseRightPressed() {
        return InputDevice::GetInstance().GetMouseRightPressed();
	}

    inline long GetMouseWheel() {
        return InputDevice::GetInstance().GetMouseWheel();
    }
}