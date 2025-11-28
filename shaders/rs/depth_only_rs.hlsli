#ifndef DEPTH_ONLY_RS_HLSLI
#define DEPTH_ONLY_RS_HLSLI

#include "../common/common.hlsli"

struct ShadowTransformCb
{
    mat4 MeshViewProj;
};

struct PointShadowTransformCb
{
    mat4 Mesh;
    vec3 LightPosition;
    float MaxDistance;
    float Flip;
    float Padding[3];
};

struct DepthOnlyObjectIdCb
{
    uint32 Id;
};

struct DepthOnlyTransformCb
{
    mat4 MeshViewProj;
    mat4 PrevFrameMVP;
};

#define SHADOW_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"          DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define POINT_SHADOW_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"          DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=24, b0, visibility=SHADER_VISIBILITY_VERTEX)"

#define DEPTH_ONLY_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
"CBV(b2)"

#define ANIMATED_DEPTH_ONLY_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=1, b1, visibility=SHADER_VISIBILITY_PIXEL), " \
"RootConstants(num32BitConstants=32, b0, visibility=SHADER_VISIBILITY_VERTEX), " \
"CBV(b2), " \
"SRV(t0)"

#define DepthOnlyRsObjectId             0
#define DepthOnlyRsMvp                  1
#define DepthOnlyRsCamera               2
#define DepthOnlyRsPrevFramePositions   3

#define ShadowRsMvp                     0


#endif