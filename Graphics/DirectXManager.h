#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class DirectXManager {
public:
    DirectXManager() = default;
    ~DirectXManager();

    // 初期化
    void Initialize();

    // ゲッター
    ID3D12Device* GetDevice()  const { return device_; }
    IDXGIFactory7* GetFactory() const { return dxgiFactory_; }

private:
    // 各初期化処理を分割
    void CreateFactory();
    void SelectAdapter();
    void CreateDevice();

    IDXGIFactory7* dxgiFactory_ = nullptr;
    IDXGIAdapter4* useAdapter_ = nullptr;
    ID3D12Device* device_ = nullptr;
};