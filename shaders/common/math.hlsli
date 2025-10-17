#ifndef MATH_HLSLI
#define MATH_HLSLI

static const float PI = 3.141592653589793238462643383279f;
static const float INV_PI = 0.3183098861837907f;
static const float INV_2PI = 0.1591549430918953f;
static const float INV_4PI = 0.07957747154594767f;
static const float2 INV_ATAN = float2(INV_2PI, INV_PI);

static float inverseLerp(float a, float b, float v)
{
    return (v - a) / (b - a);
}

static float remap(float a, float b, float c, float d, float v)
{
    return lerp(d, v, inverseLerp(b, c, a));
}

#endif