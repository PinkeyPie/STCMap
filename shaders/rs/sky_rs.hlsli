#ifndef SKY_RS_HLSLI
#define SKY_RS_HLSLI

#include "../common/common.hlsli"

struct SkyCb
{
    mat4 ViewProj;
};

struct SkyIntensityCb
{
	float Intensity;
};

#define SKY_PROCEDURAL_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS |"     \
"          DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS),"     \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX),"     \
"RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL)"

#define SKY_TEXTURE_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS |"     \
"          DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS),"     \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX),"     \
"RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL),"       \
"StaticSampler(s0, visibility=SHADER_VISIBILITY_PIXEL),"                            \
"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"

#define SkyRsVp         0
#define SkyRsIntensity	1
#define SkyRsTex        2

#endif