#include "Object3d.hlsli"

StructuredBuffer<float4x4> gWvpMatrices : register(t1);
StructuredBuffer<float4>   gColors      : register(t2);

// SV_InstanceIDはStartInstanceLocationでオフセットされないため、CPUから渡す
cbuffer InstanceOffset : register(b1)
{
    uint gInstanceOffset;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal   : NORMAL;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    uint idx = instanceId + gInstanceOffset;
    VertexShaderOutput output;
    output.position = mul(input.position, gWvpMatrices[idx]);
    output.texcoord = input.texcoord;
    output.color    = gColors[idx];
    output.normal   = input.normal;
    return output;
}