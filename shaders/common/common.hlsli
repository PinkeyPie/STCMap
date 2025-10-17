#ifndef COMMON_HLSL
#define COMMON_HLSL

#ifndef _MSC_VER
#define HLSL
#endif

#ifdef HLSL
#define uint32 uint
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mat2 float2x2
#define mat3 float3x3
#define mat4 float4x4
#else
#include "../core/math.h"
#endif

#endif