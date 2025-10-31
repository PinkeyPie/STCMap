#ifndef OUTLINE_RS_HLSL
#define OUTLINE_RS_HLSL

#include "../common/common.hlsli"

struct OutlineCb
{
    mat4 MeshViewProj;
    vec4 Color;
};

#define OUTLINE_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS |"\
          "DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_PIXEL_SHADER_ROOT_ACCESS),"\
"RootConstants(num32BitConstants=20, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define OutlineRsMvp 0

#endif