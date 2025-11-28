#ifndef POST_PROCESSING_RS_HLSLI
#define POST_PROCESSING_RS_HLSLI

#include "../common/common.hlsli"

#define POST_PROCESSING_BLOCK_SIZE 16

// ----------------------------------------
// BLOOM
// ----------------------------------------

struct BloomThresholdCb
{
    vec2 InvDimensions;
    float Threshold;
};

#define BLOOM_THRESHOLD_RS \
"RootFlags(0), " \
"RootConstants(num32BitConstants=3, b0),"  \
"StaticSampler(s0," \
"              addressU = TEXTURE_ADDRESS_CLAMP," \
"              addressV = TEXTURE_ADDRESS_CLAMP," \
"              addressW = TEXTURE_ADDRESS_CLAMP," \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)," \
"DescriptorTable(UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1))"

#define BloomThresholdRsCb           0
#define BloomThresholdRsTextures     1

struct BloomCombineCb
{
    vec2 InvDimensions;
    float Strength;
};

#define BLOOM_COMBINE_RS \
"RootFlags(0), " \
"RootConstants(num32BitConstants=3, b0),"  \
"StaticSampler(s0," \
"              addressU = TEXTURE_ADDRESS_CLAMP," \
"              addressV = TEXTURE_ADDRESS_CLAMP," \
"              addressW = TEXTURE_ADDRESS_CLAMP," \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)," \
"DescriptorTable(UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 2))"

#define BloomCombineRsCb           0
#define BloomCombineRsTextures     1

// ----------------------------------------
// BLIT
// ----------------------------------------

struct BlitCb
{
    vec2 InvDimensions;
};

#define BLIT_RS \
"RootFlags(0), " \
"RootConstants(b0, num32BitConstants = 2), " \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )," \
"StaticSampler(s0," \
"              addressU = TEXTURE_ADDRESS_CLAMP," \
"              addressV = TEXTURE_ADDRESS_CLAMP," \
"              addressW = TEXTURE_ADDRESS_CLAMP," \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define BlitRsCb                    0
#define BlitRsTextures              1

// ----------------------------------------
// GAUSSIAN BLUR
// ----------------------------------------

struct GaussianBlurCb
{
    vec2 InvDimensions;
    float StepScale;
    uint32 DirectionAndSourceMipLevel; // Direction (0 is horizontal, 1 is vertical) | sourceMipLevel.
};

#define GAUSSIAN_BLUR_RS \
"RootFlags(0), " \
"RootConstants(b0, num32BitConstants = 4), " \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )," \
"StaticSampler(s0," \
"              addressU = TEXTURE_ADDRESS_CLAMP," \
"              addressV = TEXTURE_ADDRESS_CLAMP," \
"              addressW = TEXTURE_ADDRESS_CLAMP," \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define GaussianBlurRsCb           0
#define GaussianBlurRsTextures     1

// ----------------------------------------
// HIERARCHICAL LINEAR DEPTH
// ----------------------------------------

struct HierarchicalLinearDepthCb
{
    vec2 InvDimensions;
};

#define HIERARCHICAL_LINEAR_DEPTH_RS \
"RootFlags(0), " \
"RootConstants(b0, num32BitConstants = 2), " \
"CBV(b1), " \
"DescriptorTable( UAV(u0, numDescriptors = 6), SRV(t0, numDescriptors = 1) )," \
"StaticSampler(s0," \
"              addressU = TEXTURE_ADDRESS_CLAMP," \
"              addressV = TEXTURE_ADDRESS_CLAMP," \
"              addressW = TEXTURE_ADDRESS_CLAMP," \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define HierarchicalLinearDepthRsCb           0
#define HierarchicalLinearDepthRsCamera       1
#define HierarchicalLinearDepthRsTextures     2

// ----------------------------------------
// TEMPORAL ANTI-ALIASING
// ----------------------------------------

struct TaaCb
{
    vec4 ProjectionParams;
    vec2 Dimensions;
};

#define TAA_RS \
"RootFlags(0), " \
"RootConstants(num32BitConstants=8, b0),"  \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 4) ), " \
"StaticSampler(s0," \
"              addressU = TEXTURE_ADDRESS_CLAMP," \
"              addressV = TEXTURE_ADDRESS_CLAMP," \
"              addressW = TEXTURE_ADDRESS_CLAMP," \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define TaaRsCb               0
#define TaaRsTextures         1

// ----------------------------------------
// SPECULAR AMBIENT
// ----------------------------------------

struct SpecularAmbientCb
{
    vec2 InvDimensions;
};

#define SPECULAR_AMBIENT_RS \
"RootFlags(0), " \
"RootConstants(b0, num32BitConstants = 2), " \
"CBV(b1), " \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 6) )," \
"StaticSampler(s0," \
"              addressU = TEXTURE_ADDRESS_CLAMP," \
"              addressV = TEXTURE_ADDRESS_CLAMP," \
"              addressW = TEXTURE_ADDRESS_CLAMP," \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define SpecularAmbientRsCb           0
#define SpecularAmbientRsCamera       1
#define SpecularAmbientRsTextures     2

// ----------------------------------------
// TONEMAPPING
// ----------------------------------------

struct TonemapCb
{
    float A; // Shoulder strength.
    float B; // Linear strength.
    float C; // Linear angle.
    float D; // Toe strength.
    float E; // Toe Numerator.
    float F; // Toe denominator.
    // Note E/F = Toe angle.
    float LinearWhite;

    float Exposure;
};

static TonemapCb DefaultTonemapParameters()
{
    TonemapCb result;
    result.Exposure = 0.2f;
    result.A = 0.22f;
    result.B = 0.3f;
    result.C = 0.1f;
    result.D = 0.2f;
    result.E = 0.01f;
    result.F = 0.3f;
    result.LinearWhite = 11.2f;
    return result;
}

static float AcesFilmic(float x, float A, float B, float C, float D, float E, float F)
{
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

static float FilmicTonemapping(float color, TonemapCb tonemap)
{
    float expExposure = exp2(tonemap.Exposure);
    color *= expExposure;

    float r = AcesFilmic(color, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F) /
        AcesFilmic(tonemap.LinearWhite, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F);

    return r;
}

#define TONEMAP_RS \
"RootFlags(0), " \
"RootConstants(num32BitConstants=8, b0),"  \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"

#define TonemapRsCb               0
#define TonemapRsTextures         1

// ----------------------------------------
// PRESENT
// ----------------------------------------

#define PRESENT_SDR 0
#define PRESENT_HDR 1

struct PresentCb
{
    uint32 DisplayMode;
    float StandardNits;
    float SharpenStrength;
    uint32 Offset; // x-offset | y-offset.
};

#define PRESENT_RS \
"RootFlags(0), " \
"RootConstants(num32BitConstants=4, b0),"  \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) )"

#define PresentRsCb               0
#define PresentRsTextures         1

#endif