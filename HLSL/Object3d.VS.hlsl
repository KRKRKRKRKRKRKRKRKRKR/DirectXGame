#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t1);
StructuredBuffer<float4>               gColors                 : register(t2);

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
    output.position = mul(input.position, gTransformationMatrices[idx].WVP);
    output.texcoord = input.texcoord;
    output.color    = gColors[idx];
    output.normal   = mul(input.normal, (float3x3)gTransformationMatrices[idx].World);
    return output;
}