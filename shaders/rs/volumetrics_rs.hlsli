#ifndef VOLUMETRICS_RS_HLSLI
#define VOLUMETRICS_RS_HLSLI

#include "../common/common.hlsli"

struct VolumetricsTransformCb
{
	mat4 MVP;
};

struct VolumetricsBoundingBoxCb
{
	vec4 MinCorner;
	vec4 MaxCorner;
};

#define VOLUMETRICS_RS                                                              \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS |"     \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS),"           \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX),"     \
"RootConstants(num32BitConstants=8, b1, visibility=SHADER_VISIBILITY_PIXEL),"       \
"CBV(b2, visibility=SHADER_VISIBILITY_PIXEL),"                                      \
"StaticSampler(s0,"                                                                 \
    "addressU = TEXTURE_ADDRESS_WRAP,"                                              \
    "addressV = TEXTURE_ADDRESS_WRAP,"                                              \
    "addressW = TEXTURE_ADDRESS_WRAP,"                                              \
    "filter = FILTER_MIN_MAG_MIP_POINT,"                                            \
    "visibility=SHADER_VISIBILITY_PIXEL),"                                          \
"StaticSampler(s1,"                                                                 \
    "addressU = TEXTURE_ADDRESS_BORDER,"                                            \
    "addressV = TEXTURE_ADDRESS_BORDER,"                                            \
    "addressW = TEXTURE_ADDRESS_BORDER,"                                            \
    "filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,"                          \
    "visibility=SHADER_VISIBILITY_PIXEL),"                                          \
"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), "  \
"DescriptorTable(SRV(t1, numDescriptors=4), visibility=SHADER_VISIBILITY_PIXEL), "  \
"CBV(b3, visibility=SHADER_VISIBILITY_PIXEL)"

#define VolumetricsRsMVP			0
#define VolumetricsRsBox			1
#define VolumetricsRsCamera		    2
#define VolumetricsRsDepthbuffer    3
#define VolumetricsRsSuncascades	4
#define VolumetricsRsSun			5

#endif