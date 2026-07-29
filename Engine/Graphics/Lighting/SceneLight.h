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
        // シーンにDirectionalLightComponentを1つも置かない場合、このデフォルト値がそのまま
        // 有効であり続ける（DirectionalLightComponentは光源を「作る」のではなく、毎フレーム
        // SyncToRenderer()でこの値を上書きしているだけのため）。以前は既定でON（1）にしていたが、
        // Hierarchyに何も無いのに画面が明るい・置いた覚えのない光源が効いているように見える
        // という混乱の原因になっていたため、明示的に置くまでは無効（0）にした
        uint32_t enableDirectional = 0;

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

        // --- Point Light（複数対応。固定長配列＋種類ごとに何番目かで場所を決める）---
        // 配列は各要素がそのまま16バイトの倍数になるようフィールドを並べている。HLSL側は
        // cbuffer内の配列要素を必ず16バイト境界から詰めるため、要素サイズを16の倍数に
        // 揃えておかないとC++側とHLSL側でオフセットがズレる（Object3d.PS.hlslのPointLightDataと
        // 完全に対応させること）
        // 128は素朴なフォワードシェーディング（全ピクセルで全128光源をループ、有効/無効を
        // 見て分岐）だとGPU負荷がそれなりに乗る規模（8個程度が軽快に扱える目安だった）。
        // 動作が重く感じたら、光源カリング等の設計変更を検討すること
        static constexpr uint32_t kMaxPointLights = 128;
        struct PointLight {
            Vector3  position  = { 0.0f, 2.0f, 0.0f };
            float    intensity = 1.0f;

            Vector3  color     = { 1.0f, 1.0f, 1.0f };
            uint32_t enabled   = 0;

            float    radius    = 5.0f;
            float    decay     = 1.0f;
            float    pad[2]    = {};
        };
        PointLight pointLights[kMaxPointLights];

        // --- Spot Light（複数対応。Point Lightと同じ理由で16バイト境界を揃えている）---
        static constexpr uint32_t kMaxSpotLights = 128;
        struct SpotLight {
            Vector3  position        = { 2.0f, 3.0f, 0.0f };
            float    intensity       = 1.0f;

            Vector3  direction       = { -1.0f, -1.0f, 0.0f };
            uint32_t enabled         = 0;

            Vector3  color           = { 1.0f, 1.0f, 1.0f };
            float    cosAngle        = 0.8f;  // 外側コーンのcos（この角度より外は完全に暗い）

            float    cosFalloffStart = 0.9f;  // 内側コーンのcos（この角度より内は完全に明るい）
            float    distance        = 7.0f;
            float    decay           = 2.0f;
            float    pad             = 0.0f;
        };
        SpotLight spotLights[kMaxSpotLights];
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

    // Point Light（複数対応）。indexがkMaxPointLights以上の場合は何もしない
    // （シーンに置ける点光源の実質上限。超えた分は静かに無視される）
    void SetPointLight(uint32_t index, bool enabled, const Vector3& position, const Vector3& color,
        float intensity, float radius, float decay) {
        if (index >= LightData::kMaxPointLights) return;
        auto& pl = data_.pointLights[index];
        pl.enabled   = enabled ? 1u : 0u;
        pl.position  = position;
        pl.color     = color;
        pl.intensity = intensity;
        pl.radius    = radius;
        pl.decay     = decay;
        Upload();
    }

    // Spot Light（複数対応）。indexがkMaxSpotLights以上の場合は何もしない
    void SetSpotLight(uint32_t index, bool enabled, const Vector3& position, const Vector3& direction,
        const Vector3& color, float intensity, float distance, float decay, float cosAngle, float cosFalloffStart) {
        if (index >= LightData::kMaxSpotLights) return;
        auto& sl = data_.spotLights[index];
        sl.enabled         = enabled ? 1u : 0u;
        sl.position        = position;
        sl.direction       = direction;
        sl.color           = color;
        sl.intensity       = intensity;
        sl.distance        = distance;
        sl.decay           = decay;
        sl.cosAngle        = cosAngle;
        sl.cosFalloffStart = cosFalloffStart;
        Upload();
    }

    LightData& GetData() { return data_; }

    // Directional/Point/SpotLightは「自分のGameObjectが存在する間だけ毎フレームSetEnableXxx(true)を
    // 呼ぶ」push方式のため、コンポーネントを削除しても誰もfalseへ戻さず、有効フラグが立ちっぱなしに
    // なってしまう（削除しても明るさが変わらないバグの原因）。SceneBase::Renderが毎フレーム、
    // 各ILightComponentをSyncToRendererする直前にこれを呼んで一旦全て無効化しておくことで、
    // 「今フレーム実際に存在するコンポーネントだけが有効」という状態を保証する。
    // トゥーン/スペキュラー/リムライトはGameObjectに紐付かずSceneLight::DrawImGuiパネルから直接
    // 設定する常駐の設定のため、ここではリセットしない（毎フレーム消してしまうとパネルの
    // トグルが効かなくなる）
    void ResetPerFrameEnableFlags() {
        data_.enableDirectional = 0;
        for (auto& pl : data_.pointLights) pl.enabled = 0;
        for (auto& sl : data_.spotLights) sl.enabled = 0;
        Upload();
    }

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
