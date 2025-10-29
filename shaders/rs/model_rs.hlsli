#ifndef MODEL_RS_HLSLI
#define MODEL_RS_HLSLI

#include "../common/common.hlsli"

struct TransformCb
{
    mat4 ModelViewProj;
    mat4 Model;
};

#define USE_ALBEDO_TEXTURE    (1 << 0)
#define USE_NORMAL_TEXTURE    (1 << 1)
#define USE_ROUGHNESS_TEXTURE (1 << 2)
#define USE_METALLIC_TEXTURE  (1 << 3)
#define USE_AO_TEXTURE        (1 << 4)

struct PbrMaterialCb
{
    vec4 AlbedoTint;
    float RoughnessOverride;
    float MetallicOverride;
    uint32 Flags;
};

#define MODEL_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX)," \
"RootConstants(num32BitConstants=8, b1, visibility=SHADER_VISIBILITY_PIXEL),"\
"StaticSampler(s0, addressU=TEXTURE_ADDRESS_WRAP, addressV=TEXTURE_ADDRESS_WRAP, addressW=TEXTURE_ADDRESS_WRAP, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL)," \
"DescriptorTable(SRV(t0, numDescriptors=4), visibility=SHADER_VISIBILITY_PIXEL),"\
"DescriptorTable(SRV(t0, space=1, numDescriptors=2), visibility=SHADER_VISIBILITY_PIXEL),"\
"StaticSampler(s1, addressU=TEXTURE_ADDRESS_CLAMP, addressV=TEXTURE_ADDRESS_CLAMP, addressW=TEXTURE_ADDRESS_CLAMP, filter=FILTER_MIN_MAG_MIP_LINEAR, visibility=SHADER_VISIBILITY_PIXEL),"\
"DescriptorTable(SRV(t0, space=2, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"

#define MODEL_DEPTH_ONLY_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_PIXEL_SHADER_ROOT_ACCESS),"\
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define ModelRsMeshViewProj         0
#define ModelRsMaterial             1
#define ModelRsPbrTextures          2
#define ModelRsEnvironmentTextures  3
#define ModelRsBrdf                 4

#endif