#ifndef LIGHT_CULLING
#define LIGHT_CULLING

#include "../common/common.hlsli"

#define LIGHT_CULLING_TILE_SIZE 16

struct LightCullingFrustumPlane
{
    vec3 N;
    float d;
};

struct LightCullingViewFrustum
{
    LightCullingFrustumPlane Planes[4]; // Left, right, top, bottom frustum planes.
};

struct FrustumCb
{
    uint32 NumThreadsX;
    uint32 NumThreadsY;
};

struct LightCullingCb
{
    uint32 NumThreadGroupsX;
    uint32 NumPointLights;
    uint32 NumSpotLights;
};

struct PointLightBoundingVolume
{
    vec3 Position;
    float Radius;
};

struct SpotLightBoundingVolume
{
    vec3 Tip;
    float Height;
    vec3 Direction;
    float Radius;
};

#define WORLD_SPACE_TILED_FRUSTUM_RS \
"RootFlags(0),"\
"CBV(b0)," \
"RootConstants(b1, num32BitConstants=2),"\
"UAV(u0)"

#define LIGHT_CULLING_RS \
"RootFlags(0),"\
"CBV(b0),"\
"RootConstants(b1, num32BitConstants=3),"\
"DescriptorTable(SRV(t0,numDescriptors=1,flags=DESCRIPTORS_VOLATILE)),"\
"DescriptorTable(SRV(t1,numDescriptors=3,flags=DESCRIPTORS_VOLATILE), UAV(u0, numDescriptors=3, flags=DESCRIPTORS_VOLATILE))"\

#define MAX_NUM_LIGHTS_PER_TILE 1024

#define WorldSpaceTiledFrustumRsCamera      0
#define WorldSpaceTiledFrustumRsCb          1
#define WorldSpaceTiledFrustumRsFrustumUav  2

#define LightCullingRsCamera                0
#define LightCullingRsCb                    1
#define LightCullingRsDepthBuffer           2
#define LightCullingRsSrvUav                3

#endif