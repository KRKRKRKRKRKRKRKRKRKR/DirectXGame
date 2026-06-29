#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "../../../Math/MathTypes.h"
#include "../ResourceFactory/ResourceFactory.h"
using Microsoft::WRL::ComPtr;

class DirectionalLight {
public:
    struct LightData {
        Vector3  direction        = { 1.0f, -1.0f, 0.0f };
        float    ambient          = 0.2f;
        Vector3  color            = { 1.0f, 1.0f, 1.0f };
        uint32_t enableLighting   = 1;
        float    halfLambertPower = 2.0f;
        float    pad[3]           = {};
    };

    void Initialize(ID3D12Device* device);

    void SetDirection(const Vector3& dir)  { data_.direction        = dir;   Upload(); }
    void SetAmbient(float ambient)         { data_.ambient          = ambient; Upload(); }
    void SetColor(const Vector3& color)    { data_.color            = color;  Upload(); }
    void SetHalfLambertPower(float power)  { data_.halfLambertPower = power;  Upload(); }

    LightData& GetData() { return data_; }

    // enableLighting=true → ライティングON用バッファ、false → OFF用バッファ
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress(bool enableLighting = true) const {
        return enableLighting ? resource_->GetGPUVirtualAddress()
                              : resourceOff_->GetGPUVirtualAddress();
    }

private:
    void Upload();

    LightData  data_{};
    LightData* mapped_    = nullptr;
    LightData* mappedOff_ = nullptr;
    ComPtr<ID3D12Resource> resource_;
    ComPtr<ID3D12Resource> resourceOff_;
};
