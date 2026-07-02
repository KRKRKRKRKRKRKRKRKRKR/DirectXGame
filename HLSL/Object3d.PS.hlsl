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

    // --- Point Light ---
    float3 gPointPosition;
    float  gPointIntensity;

    float3 gPointColor;
    uint   gEnablePoint;

    float  gPointRadius;
    float  gPointDecay;
    float2 gPad2;

    // --- Spot Light ---
    float3 gSpotPosition;
    float  gSpotIntensity;

    float3 gSpotDirection;
    uint   gEnableSpot;

    float3 gSpotColor;
    float  gSpotCosAngle;

    float  gSpotCosFalloffStart;
    float  gSpotDistance;
    float  gSpotDecay;
    float  gPad3;
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

        // ---- Point Light ----
        if (gEnablePoint != 0)
        {
            float3 toLight  = gPointPosition - input.worldPosition;
            float  distance = length(toLight);
            float3 lightDir = toLight / max(distance, 1e-5f);
            float  NdotL    = saturate(dot(normal, lightDir));

            float attenuation = pow(saturate(1.0f - distance / max(gPointRadius, 1e-4f)), gPointDecay);
            float3 radiance   = gPointColor * gPointIntensity * attenuation;

            diffuse += NdotL * radiance;

            if (gEnableSpecular != 0)
            {
                specular += CalcSpecular(normal, -lightDir, viewDir, radiance);
            }
        }

        // ---- Spot Light ----
        if (gEnableSpot != 0)
        {
            float3 toLight  = gSpotPosition - input.worldPosition;
            float  distance = length(toLight);
            float3 lightDir = toLight / max(distance, 1e-5f);
            float  NdotL    = saturate(dot(normal, lightDir));

            float distanceAttenuation = pow(saturate(1.0f - distance / max(gSpotDistance, 1e-4f)), gSpotDecay);
            float cosAngle            = dot(-lightDir, normalize(gSpotDirection));
            float spotAttenuation     = saturate((cosAngle - gSpotCosAngle) / max(gSpotCosFalloffStart - gSpotCosAngle, 1e-4f));

            float  attenuation = distanceAttenuation * spotAttenuation;
            float3 radiance    = gSpotColor * gSpotIntensity * attenuation;

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

    return output;
}
