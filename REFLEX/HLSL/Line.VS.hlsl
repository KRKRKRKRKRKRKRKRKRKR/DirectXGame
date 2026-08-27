cbuffer WVP : register(b0)
{
    float4x4 wvp;
};

struct VSInput
{
    float4 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(input.position, wvp);
    return output;
}
