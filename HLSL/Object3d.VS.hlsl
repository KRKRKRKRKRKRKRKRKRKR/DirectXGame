#include "Object3d.hlsli"

StructuredBuffer<float4x4> gWvpMatrices : register(t1);
StructuredBuffer<float4>   gColors      : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gWvpMatrices[instanceId]);
    output.texcoord = input.texcoord;
    output.color    = gColors[instanceId];
    return output;
}