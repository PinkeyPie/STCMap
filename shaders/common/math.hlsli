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

static float remap(float v, float oldL, float oldU, float newL, float newU)
{
    return lerp(newL, newU, inverseLerp(oldL, oldU, v));
}

static float SolidAngleOfSphere(float radius, float distance)
{
    // The angular radius of a sphere is p = arcsin(radius / d). 
	// The solid angle of a circular cap (projection of sphere) is 2pi * (1 - cos(p)).
	// cos(arcsin(x)) = sqrt(1 - x*x)

    float s = radius / distance;
    return 2.f * PI * (1.f - sqrt(max(0.f, 1.f - s * s)));
}

static uint Flatten2D(uint2 coord, uint2 dim) 
{
    return coord.x + coord.y * dim.x;
}

// Flattened array index to 2D array index.
static uint Unflatten2D(uint2 coord, uint width)
{
    return coord.x + coord.y * width;
}

inline bool IsSaturated(float a) { return a == saturate(a); }
inline bool IsSaturated(float2 a) { return IsSaturated(a.x) && IsSaturated(a.y); }
inline bool IsSaturated(float3 a) { return IsSaturated(a.x) && IsSaturated(a.y) && IsSaturated(a.z); }
inline bool IsSaturated(float4 a) { return IsSaturated(a.x) && IsSaturated(a.y) && IsSaturated(a.z) && IsSaturated(a.w); }

#endif