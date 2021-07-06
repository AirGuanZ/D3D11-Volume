#ifndef ENVIR_HLSL
#define ENVIR_HLSL

#include "rng.hlsl"

cbuffer EnvirLightParams
{
    float EnvirIntensity;
    int   EnvirTableWidth;
    int   EnvirTableHeight;
    int   EnvirTableSize;
};

struct EnvirAliasTableUnit
{
    float acceptProb;
    int   anotherIndex;
};

StructuredBuffer<EnvirAliasTableUnit> EnvirAliasTable;
Texture2D<float>                      EnvirAliasProbs;

Texture2D<float3> EnvirLight;
SamplerState      EnvirSampler;

int sampleEnvirAliasTable(inout uint rng)
{
    float u1 = rand_float(rng);
    float u2 = rand_float(rng);
    
    float nu = EnvirTableSize * u1;
    int   i  = min(int(nu), EnvirTableSize - 1);

    EnvirAliasTableUnit unit = EnvirAliasTable[i];
    return u2 <= unit.acceptProb ? i : unit.anotherIndex;
}

void sampleEnvirLight(inout uint rng, out float3 refToLight, out float pdf)
{
    int patchIdx = sampleEnvirAliasTable(rng);
    int patchY = patchIdx / EnvirTableWidth;
    int patchX = patchIdx % EnvirTableWidth;
    
    float patchPDF = EnvirAliasProbs[int2(patchX, patchY)];

    float u0 = float(patchX)     / EnvirTableWidth;
    float u1 = float(patchX + 1) / EnvirTableWidth;
    float v0 = float(patchY)     / EnvirTableHeight;
    float v1 = float(patchY + 1) / EnvirTableHeight;

    float cv0 = cos(PI * v0), cv1 = cos(PI * v1);
    float cvmin = min(cv0, cv1), cvmax = max(cv0, cv1);

    float cosTheta = cvmin + rand_float(rng) * (cvmax - cvmin);
    float sinTheta = sqrt(max(0.0f, 1 - cosTheta * cosTheta));
    float u = lerp(u0, u1, rand_float(rng));
    float phi = 2 * PI * u;

    float3 dir = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));
    float inPatchPDF = 1 / (2 * PI * ((u1 - u0) * (cvmax - cvmin)));

    refToLight = dir;
    pdf        = patchPDF * inPatchPDF;
}

float3 evalEnvirLight(float3 refToLight)
{
    refToLight = normalize(refToLight);

    float phi = atan2(refToLight.z, refToLight.x);
    float u = phi / (2 * PI);

    float theta = asin(refToLight.y);
    float v = 0.5f - theta / PI;

    float3 raw = EnvirLight.SampleLevel(EnvirSampler, float2(u, v), 0);
    return EnvirIntensity * raw;
}

#endif // #ifndef ENVIR_HLSL
