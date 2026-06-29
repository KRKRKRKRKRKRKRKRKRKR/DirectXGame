#include "Object3d.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState      gSampler : register(s0);

cbuffer LightData : register(b0)
{
    float3 gLightDirection;
    float  gAmbient;
    float3 gLightColor;
    uint   gEnableLighting;
    float  gHalfLambertPower;
    float3 gPad;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = input.color * gTexture.Sample(gSampler, input.texcoord);

    if (gEnableLighting != 0)
    {
        float3 normal  = normalize(input.normal);
        float  NdotL   = dot(normal, -normalize(gLightDirection));
        float  cosVal  = pow(NdotL * 0.5f + 0.5f, gHalfLambertPower);
        float  diffuse = gAmbient + (1.0f - gAmbient) * cosVal;
        output.color.rgb *= diffuse * gLightColor;
    }

    return output;
}
