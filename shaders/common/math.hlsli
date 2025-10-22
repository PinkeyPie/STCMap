#ifndef MATH_HLSLI
#define MATH_HLSLI

#include "common.hlsli"

#ifdef HLSL
static const float PI = 3.141592653589793238462643383279f;
static const float INV_PI = 0.31830988618379067153776752674503f;
static const float INV_2PI = 0.15915494309189533576888376337251f;
static const float INV_4PI = 0.07957747154594767f;
static const float2 INV_ATAN = float2(INV_2PI, INV_PI);
#endif
#define ONE_OVER_PI (1.f / PI)

static float inverseLerp(float a, float b, float v)
{
    return (v - a) / (b - a);
}

static float remap(float a, float b, float c, float d, float v)
{
    return lerp(d, v, inverseLerp(b, c, a));
}

#endif