#ifndef VOLUME_HLSL
#define VOLUME_HLSL

#include "rng.hlsl"

cbuffer VolumeParams
{
    float3 VolumeLower;        float VolumeMaxDensity;
    float3 VolumeUpper;        float VolumeInvMaxDensity;
    float3 VolumeInvExtent;    float VolumePhaseG;
    float  VolumeDensityScale; float VolumePhaseG2;
}

Texture3D<float>  Density;
Texture3D<float3> Albedo;
SamplerState      VolumeSampler;

float volumeMax3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float volumeMin3(float x, float y, float z)
{
    return min(x, min(y, z));
}

float2 intersectRayBox(float3 o, float3 d)
{
    float3 invD = 1 / d;
    float3 n = invD * (VolumeLower - o);
    float3 f = invD * (VolumeUpper - o);

    float3 minnf = min(n, f);
    float3 maxnf = max(n, f);

    float t0 = volumeMax3(minnf.x, minnf.y, minnf.z);
    float t1 = volumeMin3(maxnf.x, maxnf.y, maxnf.z);

    return float2(max(0.0f, t0), t1);
}

bool findEntry(float3 o, float3 d, out float3 entry)
{
    if(all((VolumeLower <= o) & (o <= VolumeUpper)))
    {
        entry = o;
        return true;
    }

    float2 incts = intersectRayBox(o, d);
    if(incts.x + 0.001f < incts.y)
    {
        entry = o + (incts.x + 0.001f) * d;
        return true;
    }

    return false;
}

float3 toTexCoord(float3 worldPos)
{
    return (worldPos - VolumeLower) * VolumeInvExtent;
}

float3 sampleAlbedo(float3 uvw)
{
    return pow(Albedo.SampleLevel(VolumeSampler, uvw, 0), 2.2f);
}

float sampleDensity(float3 uvw)
{
    float raw = Density.SampleLevel(VolumeSampler, uvw, 0);
    return VolumeDensityScale * raw;
}

float estimateTransmittance(float3 a, float3 b, inout uint rng)
{
    float t_max = distance(a, b);

    float result = 1, t = 0;

    for(int i = 0; i < 10000; ++i)
    {
        float dt = -log(1 - rand_float(rng)) * VolumeInvMaxDensity;
        t += dt;
        if(t >= t_max)
            break;

        float3 pos = lerp(a, b, t / t_max);
        float3 uvw = toTexCoord(pos);

        float density = sampleDensity(uvw);
        result *= 1 - density * VolumeInvMaxDensity;

        if(result < 0.001f)
            return 0;
    }

    return result;
}

float evalPhaseFunction(float u)
{
    float dem = 1 + VolumePhaseG2 - 2 * VolumePhaseG * u;
    return (1 - VolumePhaseG2) / (4 * PI * dem * sqrt(dem));
}

float3 samplePhaseFunction(float3 wo, inout uint rng)
{
    float s = 2 * rand_float(rng) - 1;

    float u;
    if(abs(VolumePhaseG) < 0.001f)
        u = s;
    else
    {
        float m = (1 - VolumePhaseG2) / (1 + VolumePhaseG * s);
        u = (1 + VolumePhaseG2 - m * m) / (2 * VolumePhaseG);
    }

    float cosTheta = -u;
    float sinTheta = sqrt(max(0.0f, 1 - cosTheta * cosTheta));
    float phi = 2 * PI * rand_float(rng);

    float3 localWi = float3(
        sinTheta * sin(phi),
        sinTheta * cos(phi),
        cosTheta);

    float3 X = float3(1, 0, 0);
    float3 Y = float3(0, 1, 0);

    float3 localZ = wo;
    float3 localX = normalize(cross(
        localZ, abs(dot(localZ, Y)) > 0.9f ? X : Y));
    float3 localY = cross(localZ, localX);

    return localWi.z * localZ + localWi.x * localX + localWi.y * localY;
}

bool deltaTrack(float3 a, float3 b, inout uint rng, out float3 scatter_pos)
{
    float t_max = distance(a, b), t = 0;

    for(int i = 0; i < 10000; ++i)
    {
        float dt = -log(1 - rand_float(rng)) * VolumeInvMaxDensity;
        t += dt;
        if(t >= t_max)
            break;

        float3 pos = lerp(a, b, t / t_max);
        float3 uvw = toTexCoord(pos);
        float density = sampleDensity(uvw);
        if(rand_float(rng) < density * VolumeInvMaxDensity)
        {
            scatter_pos = pos;
            return true;
        }
    }

    return false;
}

#endif // #ifndef VOLUME_HLSL
