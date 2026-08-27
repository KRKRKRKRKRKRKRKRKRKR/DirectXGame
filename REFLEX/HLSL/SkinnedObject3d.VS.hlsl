#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    // Worldの逆転置行列。不均等スケールでも法線を正しく変換するために使う
    float4x4 WorldInverseTranspose;
};
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t1);
StructuredBuffer<float4>               gColors                 : register(t2);

// SV_InstanceIDはStartInstanceLocationでオフセットされないため、CPUから渡す
cbuffer InstanceOffset : register(b1)
{
    uint gInstanceOffset;
};

// ボーン行列パレット（CPU側で毎フレーム、または初回バインドポーズ時に書き込む）
struct BoneMatrix
{
    float4x4 skinMatrix;
};
StructuredBuffer<BoneMatrix> gBoneMatrices : register(t3);

struct SkinnedVertexShaderInput
{
    float4 position    : POSITION0;
    float2 texcoord    : TEXCOORD0;
    float3 normal      : NORMAL;
    uint4  boneIndices : BLENDINDICES0;
    float4 boneWeights : BLENDWEIGHT0;
};

VertexShaderOutput main(SkinnedVertexShaderInput input, uint instanceId : SV_InstanceID)
{
    uint idx = instanceId + gInstanceOffset;

    // 最大4本のボーン行列をウェイトで加重合成
    float4x4 skinMatrix =
        gBoneMatrices[input.boneIndices.x].skinMatrix * input.boneWeights.x +
        gBoneMatrices[input.boneIndices.y].skinMatrix * input.boneWeights.y +
        gBoneMatrices[input.boneIndices.z].skinMatrix * input.boneWeights.z +
        gBoneMatrices[input.boneIndices.w].skinMatrix * input.boneWeights.w;

    float4 skinnedPosition = mul(input.position, skinMatrix);
    // 非一様スケールがない前提で法線もskinMatrixで代用（簡略化）
    float3 skinnedNormal   = mul(input.normal, (float3x3)skinMatrix);

    VertexShaderOutput output;
    output.position      = mul(skinnedPosition, gTransformationMatrices[idx].WVP);
    output.texcoord      = input.texcoord;
    output.color         = gColors[idx];
    output.normal        = normalize(mul(skinnedNormal, (float3x3)gTransformationMatrices[idx].WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrices[idx].World).xyz;
    return output;
}
