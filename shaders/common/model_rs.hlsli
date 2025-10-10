#ifndef MODEL_RS_HLSLI
#define MODEL_RS_HLSLI

#include "common.hlsli"

struct TransformCb
{
    mat4 Mvp;
    mat4 M;
};

#define MODEL_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define MODEL_RS_MVP_CBV_PARAM 0

#endif