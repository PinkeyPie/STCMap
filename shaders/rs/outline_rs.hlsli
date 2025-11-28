#ifndef OUTLINE_RS_HLSL
#define OUTLINE_RS_HLSL

#include "../common/common.hlsli"

struct OutlineMarkerCb
{
    mat4 MVP;
};

struct OutlineDrawerCb
{
    int Width, Height;
};

#define OUTLINE_MARKER_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"          DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define OUTLINE_DRAWER_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"          DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(b0, num32BitConstants = 2, visibility=SHADER_VISIBILITY_PIXEL), " \
"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"

#define OutlineRsMvp        0   

#define OutlineRsCb         0
#define OutlineRsStencil    1

#endif