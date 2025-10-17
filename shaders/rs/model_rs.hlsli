#ifndef MODEL_RS_HLSLI
#define MODEL_RS_HLSLI

#include "../common/common.hlsli"

struct TransformCb
{
    mat4 Mvp;
    mat4 M;
};

#define MODEL_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)" \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX)," \
"StaticSampler(s0, addressU=TEXTURE_ADDRESS_WRAP," \
"addressV=TEXTURE_ADDRESS_WRAP, addressW=TEXTURE_ADDRESS_WRAP, filter=FILTER_MIN_MAG_MIP_LINEAR,"\
"visibility=SHADER_VISIBILITY_PIXEL)," \
"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"

#define MODEL_RS_MVP_CBV_PARAM 0
#define MODEL_RS_ALBEDO 1

#endif