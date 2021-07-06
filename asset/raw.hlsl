#include "envir.hlsl"
#include "volume.hlsl"

cbuffer CSParams
{
    float3 Eye;      int MaxTraceDepth;
    float3 FrustumA; int OutputWidth;
    float3 FrustumB; int OutputHeight;
    float3 FrustumC; int DiscardHistory;
    float3 FrustumD;
}

Texture2D<uint>   OldRandomSeeds;
RWTexture2D<uint> NewRandomSeeds;

Texture2D<float4>   History;
RWTexture2D<float4> Output;

float3 estimateDirectIllum(float3 o, float3 wo, inout uint rng)
{
    float3 wi; float pdf;
    sampleEnvirLight(rng, wi, pdf);

    float trans = 1;
    float2 incts = intersectRayBox(o, wi);
    if(incts.x < incts.y)
        trans = estimateTransmittance(o + incts.x * wi, o + incts.y * wi, rng);
    
    float phase = evalPhaseFunction(-dot(wo, wi));

    float3 rad = evalEnvirLight(wi);
    
    return rad * trans * phase / pdf;
}

float3 trace(float3 o, float3 d, inout uint rng)
{
    float3 coef   = float3(1, 1, 1);
    float3 result = float3(0, 0, 0);

    for(int i = 0; i < MaxTraceDepth; ++i)
    {
        float2 incts = intersectRayBox(o, d);
        if(incts.x + 0.001 >= incts.y)
        {
            if(i == 0)
                result = evalEnvirLight(d);
            break;
        }

        float3 a = o, b = o + (incts.y - 0.001) * d;

        float3 scatter_pos;
        if(!deltaTrack(a, b, rng, scatter_pos))
        {
            if(i == 0)
                result = evalEnvirLight(d);
            break;
        }

        float3 uvw    = toTexCoord(scatter_pos);
        float3 albedo = sampleAlbedo(uvw);
        coef *= albedo;

        result += coef * estimateDirectIllum(scatter_pos, -d, rng);

        o = scatter_pos;
        d = samplePhaseFunction(-d, rng);
    }

    return result;
}

float4 accumulate(float3 d, inout uint rng)
{
    float3 o;
    if(!findEntry(Eye, d, o))
        return float4(evalEnvirLight(d), 1);
    
    float4 result = float4(0, 0, 0, 0);
    for(int i = 0; i < 2; ++i)
        result += float4(trace(o, d, rng), 1);
    return result;
}

[numthreads(16, 16, 1)]
void CSMain(int2 threadIdx : SV_DispatchThreadID)
{
    if(threadIdx.x >= OutputWidth || threadIdx.y >= OutputHeight)
        return;

    float2 texCoord = (threadIdx + 0.5) / float2(OutputWidth, OutputHeight);
    float3 d = normalize(lerp(
        lerp(FrustumA, FrustumB, texCoord.x),
        lerp(FrustumC, FrustumD, texCoord.x), texCoord.y));

    uint rng = OldRandomSeeds.Load(int3(threadIdx, 0));

    float4 accu = accumulate(d, rng);

    float4 history;
    if(!DiscardHistory)
        history = History[threadIdx];
    else
        history = float4(0, 0, 0, 0);

    NewRandomSeeds[threadIdx] = rng;
    Output[threadIdx] = history + accu;
}
