#include "DirectionalLight.h"
#include <cassert>

void DirectionalLight::Initialize(ID3D12Device* device) {
    resource_ = ResourceFactory::CreateBufferResource(device, sizeof(LightData));
    assert(resource_);
    HRESULT hr = resource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped_));
    assert(SUCCEEDED(hr) && mapped_);
    Upload();
}

void DirectionalLight::Upload() {
    if (mapped_) {
        *mapped_ = data_;
    }
}
