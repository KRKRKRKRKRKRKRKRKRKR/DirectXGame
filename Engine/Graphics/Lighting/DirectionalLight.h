#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "../../../Math/MathTypes.h"
#include "../ResourceFactory/ResourceFactory.h"
using Microsoft::WRL::ComPtr;

class DirectionalLight {
public:
    struct LightData {
        Vector3 direction = { 1.0f, -1.0f, 0.0f }; // ワールド空間での光の向き
        float   ambient   = 0.2f;
        Vector3 color     = { 1.0f, 1.0f, 1.0f };
        float   padding   = 0.0f;                   // 16byte アライメント用
    };

    void Initialize(ID3D12Device* device);

    // CPU 側データを更新
    void SetDirection(const Vector3& dir) { data_.direction = dir; Upload(); }
    void SetAmbient(float ambient)        { data_.ambient   = ambient; Upload(); }
    void SetColor(const Vector3& color)   { data_.color     = color; Upload(); }

    LightData& GetData() { return data_; }

    // b0 として SetGraphicsRootConstantBufferView に渡す GPU アドレス
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return resource_->GetGPUVirtualAddress(); }

private:
    void Upload();

    LightData  data_{};
    LightData* mapped_ = nullptr;
    ComPtr<ID3D12Resource> resource_;
};
