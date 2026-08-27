struct VertexShaderOutput
{
    float4 position      : SV_POSITION;
    float2 texcoord      : TEXCOORD0;
    float4 color         : COLOR;
    float3 normal        : NORMAL;
    float3 worldPosition : TEXCOORD1;
};
