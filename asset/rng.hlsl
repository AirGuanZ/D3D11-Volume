#ifndef RNG_HLSL
#define RNG_HLSL

#include "common.hlsl"

uint rand_uint(inout uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    uint result = (word >> 22u) ^ word;

    input = result;
    return result;
}

float rand_float(inout uint input)
{
    return rand_uint(input) / 4294967295.0f;
}

#endif // #ifndef RNG_HLSL
