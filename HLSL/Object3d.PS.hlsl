#include "Object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState      gSampler : register(s0);

cbuffer LightData : register(b0)
{
    float3 gLightDirection;
    float  gAmbient;
    float3 gLightColor;
    float  gPadding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float3 normal = normalize(input.normal);
    float  NdotL  = max(0.0f, dot(normal, -normalize(gLightDirection)));
    float  diffuse = gAmbient + (1.0f - gAmbient) * NdotL;

    output.color = input.color * gTexture.Sample(gSampler, input.texcoord);
    output.color.rgb *= diffuse * gLightColor;
    return output;
}
