#ifndef LIGHT_CULLING
#define LIGHT_CULLING

#include "../common/common.hlsli"

#define LIGHT_CULLING_TILE_SIZE 16
#define MAX_NUM_LIGHTS_PER_TILE 256 // Total for point and spot lights. Per tile.
#define MAX_NUM_TOTAL_DECALS 256   // Total per frame (not per tile).

#define NUM_DECAL_BUCKETS (MAX_NUM_TOTAL_DECALS / 32)
#define TILE_LIGHT_OFFSET (NUM_DECAL_BUCKETS)

#define MAX_NUM_INDICES_PER_TILE (MAX_NUM_LIGHTS_PER_TILE + NUM_DECAL_BUCKETS)

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
    uint32 NumDecals;
};


#define WORLD_SPACE_TILED_FRUSTUM_RS \
"RootFlags(0), " \
"CBV(b0), " \
"RootConstants(b1, num32BitConstants = 2), " \
"UAV(u0)"


#define LIGHT_CULLING_RS \
"RootFlags(0), " \
"CBV(b0), " \
"RootConstants(b1, num32BitConstants = 4), " \
"DescriptorTable( SRV(t0, numDescriptors = 5), UAV(u0, numDescriptors = 3) )"

#define WorldSpaceTiledFrustumRsCamera      0
#define WorldSpaceTiledFrustumRsCb          1
#define WorldSpaceTiledFrustumRsFrustumUav  2

#define LightCullingRsCamera                0
#define LightCullingRsCb                    1
#define LightCullingRsSrvUav                2

#endif