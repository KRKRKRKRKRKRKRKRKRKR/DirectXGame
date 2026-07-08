#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "../../../Math/MathTypes.h"
#include "../ResourceFactory/ResourceFactory.h"
using Microsoft::WRL::ComPtr;

// シーン全体のライティングを1つの定数バッファ(b0)にまとめて扱うクラス。
// 平行光源・点光源・スポットライト・鏡面反射・リムライト・トゥーンシェーディングの
// パラメータを持つ。各行が16バイトになるよう Vector3 + スカラー1個の組で並べている
// （HLSLのcbufferパッキング規則に合わせるため）。
class SceneLight {
public:
    struct LightData {
        // --- Directional Light ---
        Vector3  direction        = { 0.0f, -1.0f, 0.0f };
        float    ambient          = 0.2f;

        Vector3  color            = { 1.0f, 1.0f, 1.0f };
        uint32_t enableLighting   = 1;

        float    halfLambertPower = 2.0f;
        uint32_t enableToon       = 0;
        float    toonThreshold    = 0.5f;
        uint32_t enableDirectional = 1;

        // --- Camera / Specular ---
        Vector3  cameraPosition   = {};
        float    shininess        = 20.0f;

        Vector3  specularColor    = { 1.0f, 1.0f, 1.0f };
        uint32_t enableSpecular   = 0;

        // --- Rim Light ---
        Vector3  rimColor         = { 1.0f, 1.0f, 1.0f };
        float    rimPower         = 2.0f;

        float    rimStrength      = 1.0f;
        uint32_t enableRim        = 0;
        float    pad1[2]          = {};

        // --- Point Light ---
        Vector3  pointPosition    = { 0.0f, 2.0f, 0.0f };
        float    pointIntensity   = 1.0f;

        Vector3  pointColor       = { 1.0f, 1.0f, 1.0f };
        uint32_t enablePoint      = 0;

        float    pointRadius      = 5.0f;
        float    pointDecay       = 1.0f;
        float    pad2[2]          = {};

        // --- Spot Light ---
        Vector3  spotPosition         = { 2.0f, 3.0f, 0.0f };
        float    spotIntensity        = 1.0f;

        Vector3  spotDirection        = { -1.0f, -1.0f, 0.0f };
        uint32_t enableSpot           = 0;

        Vector3  spotColor            = { 1.0f, 1.0f, 1.0f };
        float    spotCosAngle         = 0.8f;  // 外側コーンのcos（この角度より外は完全に暗い）

        float    spotCosFalloffStart  = 0.9f;  // 内側コーンのcos（この角度より内は完全に明るい）
        float    spotDistance         = 7.0f;
        float    spotDecay            = 2.0f;
        float    pad3                 = 0.0f;
    };

    void Initialize(ID3D12Device* device);

    // Directional Light
    void SetDirection(const Vector3& dir)  { data_.direction        = dir;    Upload(); }
    void SetAmbient(float ambient)         { data_.ambient          = ambient; Upload(); }
    void SetColor(const Vector3& color)    { data_.color            = color;  Upload(); }
    void SetHalfLambertPower(float power)  { data_.halfLambertPower = power;  Upload(); }

    // Toon
    void SetEnableToon(bool enable)        { data_.enableToon    = enable ? 1u : 0u; Upload(); }
    void SetToonThreshold(float threshold) { data_.toonThreshold = threshold;        Upload(); }

    // Directional Light on/off
    void SetEnableDirectional(bool enable) { data_.enableDirectional = enable ? 1u : 0u; Upload(); }

    // Camera / Specular
    void SetCameraPosition(const Vector3& pos)  { data_.cameraPosition = pos;   Upload(); }
    void SetShininess(float shininess)          { data_.shininess      = shininess; Upload(); }
    void SetEnableSpecular(bool enable)         { data_.enableSpecular = enable ? 1u : 0u; Upload(); }
    void SetSpecularColor(const Vector3& color) { data_.specularColor  = color; Upload(); }

    // Rim Light
    void SetEnableRim(bool enable)         { data_.enableRim   = enable ? 1u : 0u; Upload(); }
    void SetRimColor(const Vector3& color) { data_.rimColor    = color;    Upload(); }
    void SetRimPower(float power)          { data_.rimPower    = power;    Upload(); }
    void SetRimStrength(float strength)    { data_.rimStrength = strength; Upload(); }

    // Point Light
    void SetEnablePointLight(bool enable)     { data_.enablePoint    = enable ? 1u : 0u; Upload(); }
    void SetPointPosition(const Vector3& pos) { data_.pointPosition  = pos;   Upload(); }
    void SetPointColor(const Vector3& color)  { data_.pointColor     = color; Upload(); }
    void SetPointIntensity(float intensity)   { data_.pointIntensity = intensity; Upload(); }
    void SetPointRadius(float radius)         { data_.pointRadius    = radius; Upload(); }
    void SetPointDecay(float decay)           { data_.pointDecay     = decay; Upload(); }

    // Spot Light
    void SetEnableSpotLight(bool enable)      { data_.enableSpot    = enable ? 1u : 0u; Upload(); }
    void SetSpotPosition(const Vector3& pos)  { data_.spotPosition  = pos;  Upload(); }
    void SetSpotDirection(const Vector3& dir) { data_.spotDirection = dir; Upload(); }
    void SetSpotColor(const Vector3& color)   { data_.spotColor     = color; Upload(); }
    void SetSpotIntensity(float intensity)    { data_.spotIntensity = intensity; Upload(); }
    void SetSpotDistance(float distance)      { data_.spotDistance  = distance; Upload(); }
    void SetSpotDecay(float decay)            { data_.spotDecay     = decay; Upload(); }
    void SetSpotConeAngles(float cosAngle, float cosFalloffStart) {
        data_.spotCosAngle        = cosAngle;
        data_.spotCosFalloffStart = cosFalloffStart;
        Upload();
    }

    LightData& GetData() { return data_; }

    // Toon Shading/Specular/Rim Lightは特定オブジェクトの位置・向きを持たない、シーン全体の
    // シェーディング設定のため、GameObject/コンポーネントには乗せずSceneLight自身がImGuiを持つ
    // （AudioManager::DrawImGui()と同じ「データを持つクラス自身が描画する」パターン）
    void DrawImGui();

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
