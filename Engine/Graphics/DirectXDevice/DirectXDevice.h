#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;
class DirectXDevice {
public:
    void Initialize();
    ID3D12Device* GetDevice() const { return device_.Get(); }
    IDXGIAdapter4* GetUseAdapter() const { return useAdapter_.Get(); }
    IDXGIFactory7* GetDxgiFactory() const { return dxgiFactory_.Get(); }
    void Finalize();

private:
    ComPtr<IDXGIFactory7> dxgiFactory_;
    ComPtr<IDXGIAdapter4> useAdapter_;
    ComPtr<ID3D12Device> device_;

    void InitializeCOM();
    void CreateFactory();
    void SelectAdapter();
    void CreateDevice();
    void FinalizeCOM();
};