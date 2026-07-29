#include "Object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState      gSampler : register(s0);

// BlendMode.h と対応する値。kMultiply/kScreenはBlendFactor(定数)ではSrcColor依存の
// DestBlendを補間できないため、PS側でSrcColorをgBlendStrengthで補間して代用する
static const uint kBlendModeMultiply = 4;
static const uint kBlendModeScreen   = 5;

cbuffer BlendData : register(b2)
{
    uint  gBlendMode;
    float gBlendStrength;
    uint  gEnableAlphaTest; // 2値抜き(Binary Alpha/αTest)。trueならgAlphaThreshold未満のαをdiscardする
    float gAlphaThreshold;
};

// SceneLight.h の PointLight/SpotLight と1対1で対応させること（フィールド順・16バイト境界も含む）。
// cbuffer内の配列要素は必ず16バイト境界から詰まるため、各構造体のサイズは16の倍数にしてある
static const uint kMaxPointLights = 128;
static const uint kMaxSpotLights  = 128;

struct PointLightData
{
    float3 position;
    float  intensity;

    float3 color;
    uint   enabled;

    float  radius;
    float  decay;
    float2 pad;
};

struct SpotLightData
{
    float3 position;
    float  intensity;

    float3 direction;
    uint   enabled;

    float3 color;
    float  cosAngle;

    float  cosFalloffStart;
    float  distance;
    float  decay;
    float  pad;
};

cbuffer LightData : register(b0)
{
    // --- Directional Light ---
    float3 gLightDirection;
    float  gAmbient;

    float3 gLightColor;
    uint   gEnableLighting;

    float  gHalfLambertPower;
    uint   gEnableToon;
    float  gToonThreshold;
    uint   gEnableDirectional;

    // --- Camera / Specular ---
    float3 gCameraPosition;
    float  gShininess;

    float3 gSpecularColor;
    uint   gEnableSpecular;

    // --- Rim Light ---
    float3 gRimColor;
    float  gRimPower;

    float  gRimStrength;
    uint   gEnableRim;
    float2 gPad1;

    // --- Point Light（複数対応）---
    PointLightData gPointLights[kMaxPointLights];

    // --- Spot Light（複数対応）---
    SpotLightData gSpotLights[kMaxSpotLights];
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 平行光源/点光源/スポットライト共通の拡散反射(Half-Lambert or Toon)計算
float CalcDiffuseFactor(float NdotL)
{
    float cosVal = pow(NdotL * 0.5f + 0.5f, gHalfLambertPower);
    if (gEnableToon != 0)
    {
        return cosVal >= gToonThreshold ? 1.0f : gAmbient;
    }
    return gAmbient + (1.0f - gAmbient) * cosVal;
}

// Blinn-Phongの鏡面反射
float3 CalcSpecular(float3 normal, float3 lightDir, float3 viewDir, float3 lightColor)
{
    float3 halfVector = normalize(-lightDir + viewDir);
    float  NdotH      = saturate(dot(normal, halfVector));
    return pow(NdotH, gShininess) * gSpecularColor * lightColor;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = input.color * gTexture.Sample(gSampler, input.texcoord);

#ifdef ENABLE_ALPHA_TEST
    // 2値抜き(Binary Alpha/αTest): αがしきい値未満のピクセルは描画せず棄却する
    // 微小なイプシロンを加えるのは、本来1.0のはずのαがテクスチャの量子化/丸め誤差で
    // 0.999...になることがあり、しきい値=1.0のときに完全不透明ピクセルまで
    // 誤ってdiscardされてしまうのを防ぐため
    //
    // discardをコンパイル時に完全に排除するため#ifdefで分岐している（gEnableAlphaTestという
    // 実行時フラグだけの分岐だと、シェーダー内にdiscard命令が静的に存在するだけでGPUドライバの
    // Early-Z最適化が無効化され、αTestを使わないオブジェクトまでオーバードロー削減の恩恵を失うため）
    if (gEnableAlphaTest != 0 && output.color.a + 1e-5f < gAlphaThreshold)
    {
        discard;
    }
#endif

    if (gEnableLighting != 0)
    {
        float3 normal  = normalize(input.normal);
        float3 viewDir = normalize(gCameraPosition - input.worldPosition);

        float3 diffuse  = float3(0.0f, 0.0f, 0.0f);
        float3 specular = float3(0.0f, 0.0f, 0.0f);

        // ---- Directional Light ----
        if (gEnableDirectional != 0)
        {
            float3 lightDir = normalize(gLightDirection);
            float  NdotL    = dot(normal, -lightDir);
            diffuse += CalcDiffuseFactor(NdotL) * gLightColor;

            if (gEnableSpecular != 0)
            {
                specular += CalcSpecular(normal, lightDir, viewDir, gLightColor);
            }
        }

        // ---- Point Lights（複数対応、有効なものだけ加算）----
        for (uint pi = 0; pi < kMaxPointLights; ++pi)
        {
            if (gPointLights[pi].enabled == 0) continue;

            float3 toLight  = gPointLights[pi].position - input.worldPosition;
            float  distance = length(toLight);
            float3 lightDir = toLight / max(distance, 1e-5f);
            float  NdotL    = saturate(dot(normal, lightDir));

            float attenuation = pow(saturate(1.0f - distance / max(gPointLights[pi].radius, 1e-4f)), gPointLights[pi].decay);
            float3 radiance   = gPointLights[pi].color * gPointLights[pi].intensity * attenuation;

            diffuse += NdotL * radiance;

            if (gEnableSpecular != 0)
            {
                specular += CalcSpecular(normal, -lightDir, viewDir, radiance);
            }
        }

        // ---- Spot Lights（複数対応、有効なものだけ加算）----
        for (uint si = 0; si < kMaxSpotLights; ++si)
        {
            if (gSpotLights[si].enabled == 0) continue;

            float3 toLight  = gSpotLights[si].position - input.worldPosition;
            float  distance = length(toLight);
            float3 lightDir = toLight / max(distance, 1e-5f);
            float  NdotL    = saturate(dot(normal, lightDir));

            float distanceAttenuation = pow(saturate(1.0f - distance / max(gSpotLights[si].distance, 1e-4f)), gSpotLights[si].decay);
            float cosAngle            = dot(-lightDir, normalize(gSpotLights[si].direction));
            float spotAttenuation     = saturate((cosAngle - gSpotLights[si].cosAngle) / max(gSpotLights[si].cosFalloffStart - gSpotLights[si].cosAngle, 1e-4f));

            float  attenuation = distanceAttenuation * spotAttenuation;
            float3 radiance    = gSpotLights[si].color * gSpotLights[si].intensity * attenuation;

            diffuse += NdotL * radiance;

            if (gEnableSpecular != 0)
            {
                specular += CalcSpecular(normal, -lightDir, viewDir, radiance);
            }
        }

        output.color.rgb *= diffuse;
        output.color.rgb += specular;

        // ---- Rim Light ----
        if (gEnableRim != 0)
        {
            float rim = pow(1.0f - saturate(dot(normal, viewDir)), gRimPower) * gRimStrength;
            output.color.rgb += rim * gRimColor;
        }
    }

    // kMultiply: Dest*SrcColor がブレンド式のため、SrcColorを白へ補間するとstrength=0で無効化できる
    // kScreen  : Src*1 + Dest*(1-SrcColor) がブレンド式のため、SrcColorを黒へ補間するとstrength=0で無効化できる
    if (gBlendMode == kBlendModeMultiply)
    {
        output.color.rgb = lerp(float3(1.0f, 1.0f, 1.0f), output.color.rgb, gBlendStrength);
    }
    else if (gBlendMode == kBlendModeScreen)
    {
        output.color.rgb = lerp(float3(0.0f, 0.0f, 0.0f), output.color.rgb, gBlendStrength);
    }
    else
    {
        // kNormal/kAdd/kSubtract: BlendStateがSrcAlpha/InvSrcAlphaでper-pixelアルファを実際に使うため、
        // テクスチャのアルファ（文字の輪郭など画素ごとの透明度）はそのまま活かしつつ、
        // gBlendStrengthは「全体の濃さ」の追加乗算として効かせる
        output.color.a *= gBlendStrength;
    }

    return output;
}
