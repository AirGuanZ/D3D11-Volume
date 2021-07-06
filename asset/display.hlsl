struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0.5, 1);
    return output;
}

cbuffer PSParams
{
    float Exposure;
};

#define A 2.51
#define B 0.03
#define C 2.43
#define D 0.59
#define E 0.14

float3 tonemap(float3 x)
{
    float3 v = Exposure * x;
    return (v * (A * v + B)) / (v * (C * v + D) + E);
}

Texture2D<float4> Tex;
SamplerState      TexSampler;

float4 PSMain(VSOutput input): SV_TARGET
{
    float4 val = Tex.SampleLevel(TexSampler, input.texCoord, 0);
    float3 result = val.a > 0 ? val.rgb / val.a : float3(0, 0, 0);
    return float4(pow(tonemap(result), 1 / 2.2f), 1);
}
