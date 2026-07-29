#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl/client.h>
#include <Xinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

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
	bool GetMouseMiddlePressed() const { return (mouseState_.rgbButtons[2] & 0x80) != 0; }
    long GetMouseWheel() const { return mouseState_.lZ; }

    // ---- ゲームパッド（XInput） ----
    // indexは0〜XUSER_MAX_COUNT-1（0始まり、既定は0番のコントローラ1台のみ想定）
    static constexpr int kGamepadCount = XUSER_MAX_COUNT;

    bool IsGamepadConnectedInternal(int index) const;
    bool IsGamepadButtonPressedInternal(int index, WORD button) const;
    bool IsGamepadButtonTriggeredInternal(int index, WORD button) const;
    float GetGamepadLeftStickXInternal(int index) const;
    float GetGamepadLeftStickYInternal(int index) const;
    float GetGamepadRightStickXInternal(int index) const;
    float GetGamepadRightStickYInternal(int index) const;
    float GetGamepadLeftTriggerInternal(int index) const;
    float GetGamepadRightTriggerInternal(int index) const;
    void SetGamepadVibrationInternal(int index, float leftMotor, float rightMotor);

private:
    InputDevice() = default;
    ~InputDevice() = default;

    //キーボード
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboardDevice_;
    BYTE key_[256] = {};
    BYTE prevKey_[256] = {};

    //マウス
    Microsoft::WRL::ComPtr<IDirectInputDevice8> mouseDevice_;
    DIMOUSESTATE mouseState_ = {};

    //ゲームパッド（XInput、DirectInputとは別API。プレイヤー分岐に備えて配列で持つ）
    XINPUT_STATE gamepadState_[kGamepadCount]{};
    XINPUT_STATE prevGamepadState_[kGamepadCount]{};
    bool         gamepadConnected_[kGamepadCount]{};

    void UpdateGamepad();
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

    inline bool IsMouseMiddlePressed() {
        return InputDevice::GetInstance().GetMouseMiddlePressed();
    }

    inline long GetMouseWheel() {
        return InputDevice::GetInstance().GetMouseWheel();
    }

    // ---- ゲームパッド（XInput） ----
    // buttonはXInputのビット定数（XINPUT_GAMEPAD_A等）をそのまま渡す
    // （DIK_WをそのままIsPressed()へ渡す既存のキーボードAPIと同じ流儀）。
    // indexは0〜3（既定0＝1台目のコントローラ、シングルプレイヤー用途ではデフォルトのままでよい）
    inline bool IsGamepadConnected(int index = 0) {
        return InputDevice::GetInstance().IsGamepadConnectedInternal(index);
    }

    inline bool IsGamepadPressed(WORD button, int index = 0) {
        return InputDevice::GetInstance().IsGamepadButtonPressedInternal(index, button);
    }

    inline bool IsGamepadTriggered(WORD button, int index = 0) {
        return InputDevice::GetInstance().IsGamepadButtonTriggeredInternal(index, button);
    }

    inline float GetGamepadLeftStickX(int index = 0) {
        return InputDevice::GetInstance().GetGamepadLeftStickXInternal(index);
    }

    inline float GetGamepadLeftStickY(int index = 0) {
        return InputDevice::GetInstance().GetGamepadLeftStickYInternal(index);
    }

    inline float GetGamepadRightStickX(int index = 0) {
        return InputDevice::GetInstance().GetGamepadRightStickXInternal(index);
    }

    inline float GetGamepadRightStickY(int index = 0) {
        return InputDevice::GetInstance().GetGamepadRightStickYInternal(index);
    }

    inline float GetGamepadLeftTrigger(int index = 0) {
        return InputDevice::GetInstance().GetGamepadLeftTriggerInternal(index);
    }

    inline float GetGamepadRightTrigger(int index = 0) {
        return InputDevice::GetInstance().GetGamepadRightTriggerInternal(index);
    }

    inline void SetGamepadVibration(float leftMotor, float rightMotor, int index = 0) {
        InputDevice::GetInstance().SetGamepadVibrationInternal(index, leftMotor, rightMotor);
    }
}