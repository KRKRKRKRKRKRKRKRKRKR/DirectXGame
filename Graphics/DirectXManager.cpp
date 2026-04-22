#include "DirectXManager.h"
#include "../Utils/Logger.h"
#include "../Utils/StringUtils.h"
#include <cassert>
#include <format>

DirectXManager::~DirectXManager() {
    if (device_) { device_->Release();      device_ = nullptr; }
    if (useAdapter_) { useAdapter_->Release();  useAdapter_ = nullptr; }
    if (dxgiFactory_) { dxgiFactory_->Release(); dxgiFactory_ = nullptr; }
}

void DirectXManager::Initialize() {
    CreateFactory();
    SelectAdapter();
    CreateDevice();
    Logger::Log("Complete Initialize DirectXManager\n");
}

void DirectXManager::CreateFactory() {
    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));
}

void DirectXManager::SelectAdapter() {
    for (UINT i = 0;
        dxgiFactory_->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&useAdapter_)) != DXGI_ERROR_NOT_FOUND;
            ++i) {

        DXGI_ADAPTER_DESC3 adapterDesc{};
        HRESULT hr = useAdapter_->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));

        // ソフトウェアアダプタを除外
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            Logger::Log(StringUtils::ConvertString(
                std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
            break;
        }
        useAdapter_ = nullptr;
    }
    assert(useAdapter_ != nullptr);
}

void DirectXManager::CreateDevice() {
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0
    };
    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

    for (size_t i = 0; i < _countof(featureLevels); i++) {
        HRESULT hr = D3D12CreateDevice(
            useAdapter_, featureLevels[i], IID_PPV_ARGS(&device_));

        if (SUCCEEDED(hr)) {
            Logger::Log(std::format(
                "FeatureLevel:{}\n", featureLevelStrings[i]));
            break;
        }
    }
    assert(device_ != nullptr);
}