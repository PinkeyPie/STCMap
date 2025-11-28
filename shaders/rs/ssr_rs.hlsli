#ifndef SSR_RS_HLSLI
#define SSR_RS_HLSLI

#include "../common/common.hlsli"

#define SSR_BLOCK_SIZE 16

#define SSR_GGX_IMPORTANCE_SAMPLE_BIAS 0.1f

struct SsrRaycastCb
{
    vec2 Dimensions;
    vec2 InvDimensions;
    uint32 FrameIndex;
    uint32 NumSteps;

    float MaxDistance;

    float StrideCutoff;
    float MinStride;
    float MaxStride;
};

#ifndef HLSLI
static SsrRaycastCb DefaultSSRParameters()
{
    SsrRaycastCb result;
    result.NumSteps = 400;
    result.MaxDistance = 1000.f;
    result.StrideCutoff = 100.f;
    result.MinStride = 5.f;
    result.MaxStride = 30.f;
    return result;
}
#endif

#define SSR_RAYCAST_RS                                                          \
"RootFlags(0), "                                                                \
"RootConstants(b0, num32BitConstants = 10), "                                   \
"CBV(b1), "                                                                     \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 5) ),"  \
"StaticSampler(s0,"                                                             \
"              addressU = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressV = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressW = TEXTURE_ADDRESS_CLAMP,"                               \
"              filter = FILTER_MIN_MAG_MIP_LINEAR), "                           \
"StaticSampler(s1,"                                                             \
"              addressU = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressV = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressW = TEXTURE_ADDRESS_CLAMP,"                               \
"              filter = FILTER_MIN_MAG_MIP_POINT)"

#define SsrRaycastRsCb           0
#define SsrRaycastRsCamera       1
#define SsrRaycastRsTextures     2

struct SsrResolveCb
{
    vec2 Dimensions;
    vec2 InvDimensions;
};

#define SSR_RESOLVE_RS                                                          \
"RootFlags(0), "                                                                \
"RootConstants(b0, num32BitConstants = 4), "                                    \
"CBV(b1), "                                                                     \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 6) ),"  \
"StaticSampler(s0,"                                                             \
"              addressU = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressV = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressW = TEXTURE_ADDRESS_CLAMP,"                               \
"              filter = FILTER_MIN_MAG_MIP_LINEAR), "                           \
"StaticSampler(s1,"                                                             \
"              addressU = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressV = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressW = TEXTURE_ADDRESS_CLAMP,"                               \
"              filter = FILTER_MIN_MAG_MIP_POINT)"

#define SsrResolveRsCb           0
#define SsrResolveRsCamera       1
#define SsrResolveRsTextures     2

struct SsrTemporalCb
{
    vec2 InvDimensions;
};

#define SSR_TEMPORAL_RS                                                         \
"RootFlags(0), "                                                                \
"RootConstants(b0, num32BitConstants = 2), "                                    \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 3) ),"  \
"StaticSampler(s0,"                                                             \
"              addressU = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressV = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressW = TEXTURE_ADDRESS_CLAMP,"                               \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define SsrTemporalRsCb           0
#define SsrTemporalRsTextures     1

struct SsrMedianBlurCb
{
    vec2 InvDimensions;
};

#define SSR_MEDIAN_BLUR_RS                                                      \
"RootFlags(0), "                                                                \
"RootConstants(b0, num32BitConstants = 2), "                                    \
"DescriptorTable( UAV(u0, numDescriptors = 1), SRV(t0, numDescriptors = 1) ),"  \
"StaticSampler(s0,"                                                             \
"              addressU = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressV = TEXTURE_ADDRESS_CLAMP,"                               \
"              addressW = TEXTURE_ADDRESS_CLAMP,"                               \
"              filter = FILTER_MIN_MAG_MIP_LINEAR)"


#define SsrMedianBlurRsCb           0
#define SsrMedianBlurRsTextures     1

#endif