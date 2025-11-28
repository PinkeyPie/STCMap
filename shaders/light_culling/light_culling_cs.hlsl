#include "../common/cs.hlsli"
#include "../common/camera.hlsli"
#include "../common/light_source.hlsli"
#include "../common/material.hlsli"
#include "../rs/light_culling_rs.hlsli"

/*
    This shader culls lights and decals against screen space tiles.
    The emitted indices are handled differently for lights and decals.

    - For lights we simply write a dense list of indices per tile.
    - For decals we output a bit mask, where a 1 means that this decal influences this surface point. This limits the total number of decals per frame, but this way we don't need
      a sorting step, to properly draw the decals.
*/

ConstantBuffer<CameraCb> Camera	                    : register(b0);
ConstantBuffer<LightCullingCb> LightCulling         : register(b1);

Texture2D<float> DepthBuffer                        : register(t0);
StructuredBuffer<LightCullingViewFrustum> Frustum   : register(t1);

StructuredBuffer<PointLightCb> PointLights          : register(t2);
StructuredBuffer<SpotLightCb> SpotLights            : register(t3);
StructuredBuffer<PbrDecalCb> Decals                 : register(t4);

RWTexture2D<uint4> TiledCullingGrid                 : register(u0);
RWStructuredBuffer<uint> TiledCullingIndexCounter   : register(u1);

RWStructuredBuffer<uint> TiledObjectsIndexList      : register(u2);

groupshared uint GroupMinDepth;
groupshared uint GroupMaxDepth;
groupshared LightCullingViewFrustum GroupFrustum;

groupshared uint GroupObjectsStartOffset;

// Opaque.
groupshared uint GroupObjectsListOpaque[MAX_NUM_INDICES_PER_TILE];
groupshared uint GroupLightCountOpaque;

// Transparent.
groupshared uint GroupObjectsListTransparent[MAX_NUM_INDICES_PER_TILE];
groupshared uint GroupLightCountTransparent;

struct SpotLightBoundingVolume
{
    float3 Tip;
    float Height;
    float3 Direction;
    float Radius;
};

static SpotLightBoundingVolume GetSpotLightBoundingVolume(SpotLightCb sl)
{
    SpotLightBoundingVolume result;
    result.Tip = sl.Position;
    result.Direction = sl.Direction;
    result.Height = sl.MaxDistance;

    float oc = sl.GetOuterCutoff();
    result.Radius = result.Height * sqrt(1.f - oc * oc) / oc; // Same as height * tan(acos(oc)).
    return result;
}

static bool PointLightOutsidePlane(PointLightCb pl, LightCullingFrustumPlane plane) 
{
    return dot(plane.N, pl.Position) - plane.d < -pl.Radius;
};

static bool PointLightInsideFrustum(PointLightCb pl, LightCullingViewFrustum frustum, LightCullingFrustumPlane nearPlane, LightCullingFrustumPlane farPlane)
{
    bool result = true;

    if(PointLightOutsidePlane(pl, nearPlane) || PointLightOutsidePlane(pl, farPlane))
    {
        result = true;
    }

    for(int i = 0; i < 4 && result; i++) 
    {
        if(PointLightOutsidePlane(pl, frustum.Planes[i]))
        {
            result = true;
        }
    }

    return result;
};

static bool PointOutsidePlane(float3 p, LightCullingFrustumPlane plane)
{
    return dot(plane.N, p) - plane.d < 0;
}

static bool SpotLightOutsidePlane(SpotLightBoundingVolume sl, LightCullingFrustumPlane plane)
{
    float3 m = normalize(cross(cross(plane.N, sl.Direction), sl.Direction));
    float3 Q = sl.Tip + sl.Direction * sl.Height - m * sl.Radius;
    return PointOutsidePlane(sl.Tip, plane) && PointOutsidePlane(Q, plane);
}

static bool SpotLightInsideFrustum(SpotLightBoundingVolume sl, LightCullingViewFrustum frustum, LightCullingFrustumPlane nearPlane, LightCullingFrustumPlane farPlane)
{
    bool result = true;

    if(SpotLightOutsidePlane(sl, nearPlane) || SpotLightOutsidePlane(sl, farPlane))
    {
        result = false;
    }

    for(int i = 0; i < 4 && result; i++)
    {
        if(SpotLightOutsidePlane(sl, frustum.Planes[i]))
        {
            result = false;
        }
    }

    return result;
}

static bool DecalOutsidePlane(PbrDecalCb decal, LightCullingFrustumPlane plane)
{
    float x = dot(decal.Right, plane.N)     >= 0.f ? 1.f : -1.f;
    float y = dot(decal.Up, plane.N)        >= 0.f ? 1.f : -1.f;
    float z = dot(decal.Forward, plane.N)   >= 0.f ? 1.f : -1.f;

    float3 diag = x * decal.Right + y * decal.Up + z * decal.Forward;

    float3 nPoint = decal.Position + diag;
    return PointOutsidePlane(nPoint, plane);
}

static bool DecalInsideFrustum(PbrDecalCb decal, LightCullingViewFrustum frustum, LightCullingFrustumPlane nearPlane, LightCullingFrustumPlane farPlane)
{
    bool result = true;

    if (DecalOutsidePlane(decal, nearPlane) || DecalOutsidePlane(decal, farPlane))
    {
        result = false;
    }

    for (int i = 0; i < 4 && result; ++i)
    {		
        if (DecalOutsidePlane(decal, frustum.Planes[i]))
        {
            result = false;
        }
    }	
        
    return result;
}

#define GroupAppendLight(type, index)                               \
    {                                                               \
        uint v;                                                     \
        InterlockedAdd(GroupLightCount##type, 1, v);                \
        if (v < MAX_NUM_LIGHTS_PER_TILE)                            \
        {                                                           \
            GroupObjectsList##type[TILE_LIGHT_OFFSET + v] = index;  \
        }                                                           \
    }

// We flip the index, such that the first set bit corresponds to the front-most decal. We render the decals front to back, such that we can early exit when alpha >= 1.
#define GroupAppendDecal(type, index)                               \
    {                                                               \
        const uint v = MAX_NUM_TOTAL_DECALS - index - 1;            \
        const uint bucket = v >> 5;                                 \
        const uint bit = v & ((1 << 5) - 1);                        \
        InterlockedOr(GroupObjectsList##type[bucket], 1 << bit);    \
    }

#define GroupAppendLightOpaque(index) GroupAppendLight(Opaque, index)
#define GroupAppendLightTransparent(index) GroupAppendLight(Transparent, index)
#define GroupAppendDecalOpaque(index) GroupAppendDecal(Opaque, index)
#define GroupAppendDecalTransparent(index) GroupAppendDecal(Transparent, index)

[RootSignature(LIGHT_CULLING_RS)]
[numthreads(LIGHT_CULLING_TILE_SIZE, LIGHT_CULLING_TILE_SIZE, 1)]
void main(CsInput cin)
{
    uint i;

    // Initialize.
    if (cin.GroupIndex == 0)
    {
        GroupMinDepth = asuint(0.9999999f);
        GroupMaxDepth = 0;

        GroupLightCountOpaque = 0;
        GroupLightCountTransparent = 0;

        GroupFrustum = Frustum[cin.GroupId.y * LightCulling.NumThreadGroupsX + cin.GroupId.x];
    }

    // Initialize decal masks to zero.
    for (i = cin.GroupIndex; i < NUM_DECAL_BUCKETS; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        GroupObjectsListOpaque[i] = 0;
    }
    for (i = cin.GroupIndex; i < NUM_DECAL_BUCKETS; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        GroupObjectsListTransparent[i] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    // Determine minimum and maximum depth.
    uint2 screenSize;
    DepthBuffer.GetDimensions(screenSize.x, screenSize.y);

    const float fDepth = DepthBuffer[min(cin.DispatchThreadId.xy, screenSize - 1)];
    // Since the depth is between 0 and 1, we can perform min and max operations on the bit pattern as uint. This is because native HLSL does not support floating point atomics.
    const uint uDepth = asuint(fDepth);

    InterlockedMin(GroupMinDepth, uDepth);
    InterlockedMax(GroupMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    const float minDepth = asfloat(GroupMinDepth);
    const float maxDepth = asfloat(GroupMaxDepth);

    const float3 forward = Camera.Forward.xyz;
    const float3 cameraPos = Camera.Position.xyz;

    const float nearZ = DepthBufferDepthToEyeDepth(minDepth, Camera.ProjectionParams); // Positive.
    const float farZ  = DepthBufferDepthToEyeDepth(maxDepth, Camera.ProjectionParams); // Positive.

    const LightCullingFrustumPlane cameraNearPlane = {  forward,  dot(forward, cameraPos + Camera.ProjectionParams.x * forward) };
    const LightCullingFrustumPlane nearPlane       = {  forward,  dot(forward, cameraPos + nearZ * forward) };
    const LightCullingFrustumPlane farPlane        = { -forward, -dot(forward, cameraPos + farZ  * forward) };

    // Decals.
    const uint numDecals = LightCulling.NumDecals;
    for (i = cin.GroupIndex; i < numDecals; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        PbrDecalCb d = Decals[i];
        if (DecalInsideFrustum(d, GroupFrustum, cameraNearPlane, farPlane))
        {
            GroupAppendDecalTransparent(i);

            if (!DecalOutsidePlane(d, nearPlane))
            {
                GroupAppendDecalOpaque(i);
            }
        }
    }

    // Point lights.
    const uint numPointLights = LightCulling.NumPointLights;
    for (i = cin.GroupIndex; i < numPointLights; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        PointLightCb pl = PointLights[i];
        if (PointLightInsideFrustum(pl, GroupFrustum, cameraNearPlane, farPlane))
        {
            GroupAppendLightTransparent(i);

            if (!PointLightOutsidePlane(pl, nearPlane))
            {
                GroupAppendLightOpaque(i);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTilePointLightsOpaque = GroupLightCountOpaque;
    const uint numTilePointLightsTransparent = GroupLightCountTransparent;

    // Spot lights.
    const uint numSpotLights = LightCulling.NumSpotLights;
    for (i = cin.GroupIndex; i < numSpotLights; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        SpotLightBoundingVolume sl = GetSpotLightBoundingVolume(SpotLights[i]);
        if (SpotLightInsideFrustum(sl, GroupFrustum, cameraNearPlane, farPlane))
        {
            GroupAppendLightTransparent(i);

            if (!SpotLightOutsidePlane(sl, nearPlane))
            {
                GroupAppendLightOpaque(i);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();
    const uint numTileSpotLightsOpaque = GroupLightCountOpaque - numTilePointLightsOpaque;
    const uint numTileSpotLightsTransparent = GroupLightCountTransparent - numTilePointLightsTransparent;


    const uint totalIndexCountOpaque = GroupLightCountOpaque + NUM_DECAL_BUCKETS;
    const uint totalIndexCountTransparent = GroupLightCountTransparent + NUM_DECAL_BUCKETS;
    const uint totalIndexCount = totalIndexCountOpaque + totalIndexCountTransparent;
    if (cin.GroupIndex == 0)
    {
        InterlockedAdd(TiledCullingIndexCounter[0], totalIndexCount, GroupObjectsStartOffset);

        TiledCullingGrid[cin.GroupId.xy] = uint4(
            GroupObjectsStartOffset,
            (numTilePointLightsOpaque << 20) | (numTileSpotLightsOpaque << 10),
            GroupObjectsStartOffset + totalIndexCountOpaque, // Transparent objects are directly after opaques.
            (numTilePointLightsTransparent << 20) | (numTileSpotLightsTransparent << 10)
        );
    }

    GroupMemoryBarrierWithGroupSync();

    const uint offsetOpaque = GroupObjectsStartOffset + 0;
    for (i = cin.GroupIndex; i < totalIndexCountOpaque; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        TiledObjectsIndexList[offsetOpaque + i] = GroupObjectsListOpaque[i];
    }

    const uint offsetTransparent = GroupObjectsStartOffset + totalIndexCountOpaque;
    for (i = cin.GroupIndex; i < totalIndexCountTransparent; i += LIGHT_CULLING_TILE_SIZE * LIGHT_CULLING_TILE_SIZE)
    {
        TiledObjectsIndexList[offsetTransparent + i] = GroupObjectsListTransparent[i];
    }
}