#include "SceneLight.h"
#include <cassert>

void SceneLight::Initialize(ID3D12Device* device) {
    resource_ = ResourceFactory::CreateBufferResource(device, sizeof(LightData));
    assert(resource_);
    HRESULT hr = resource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped_));
    assert(SUCCEEDED(hr) && mapped_);

    resourceOff_ = ResourceFactory::CreateBufferResource(device, sizeof(LightData));
    assert(resourceOff_);
    hr = resourceOff_->Map(0, nullptr, reinterpret_cast<void**>(&mappedOff_));
    assert(SUCCEEDED(hr) && mappedOff_);
    // enableLighting=0 固定（他フィールドは不使用なので初期値のまま）
    mappedOff_->enableLighting = 0;

    Upload();
}

void SceneLight::Upload() {
    if (mapped_) {
        *mapped_ = data_;
        // enableLighting は常に 1
        mapped_->enableLighting = 1;
    }
}
