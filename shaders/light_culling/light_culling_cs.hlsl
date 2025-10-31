#include "../common/cs.hlsli"
#include "../common/camera.hlsli"
#include "../rs/light_culling.hlsli"

#define BLOCK_SIZE 16

ConstantBuffer<CameraCb> Camera : register(b0);
ConstantBuffer<LightCullingCb> LightCulling : register(b1);

Texture2D<float> DepthBuffer : register(t0);

StructuredBuffer<PointLightBoundingVolume> PointLights  : register(t1);
StructuredBuffer<SpotLightBoundingVolume> SpotLights    : register(t2);
StructuredBuffer<LightCullingViewFrustum> Frustum       : register(t3);

RWStructuredBuffer<uint> OpaqueLightIndexCounter        : register(u0);
RWStructuredBuffer<uint> OpaqueLightIndexList           : register(u1);
RWTexture2D<uint2> OpaqueLightGrid                      : register(u2);

groupshared uint GroupMinDepth;
groupshared uint GroupMaxDepth;
groupshared LightCullingViewFrustum GroupFrustum;

groupshared uint OpaqueLightCount;
groupshared uint OpaqueLightIndexStartOffset;
groupshared uint OpaqueLightList[MAX_NUM_LIGHTS_PER_TILE];

static bool PointLightOutsidePlane(PointLightBoundingVolume pl, LightCullingFrustumPlane plane) 
{
    return dot(plane.N, pl.Position) - plane.d < -pl.Radius;
};

static bool PointLightInsideFrustum(PointLightBoundingVolume pl, LightCullingViewFrustum frustum, LightCullingFrustumPlane nearPlane, LightCullingFrustumPlane farPlane)
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
    float3 m = cross(cross(plane.N, sl.Direction), sl.Direction);
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

static void OpaqueAppendLight(uint lightIndex)
{
    uint index;
    InterlockedAdd(OpaqueLightCount, 1, index);
    if(index < MAX_NUM_LIGHTS_PER_TILE)
    {
        OpaqueLightList[index] = lightIndex;
    }
}

[RootSignature(LIGHT_CULLING_RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput cin)
{
    uint2 texCoord = cin.DispatchThreadId.xy;
    float fDepth = DepthBuffer.Load(uint3(texCoord, 0)).r;
    // Since the depth is between 0 and 1, we can perform min and max operations on the bit pattern as uint. This is because native HLSL does not support floating point atomics
    uint uDepth = asuint(fDepth);

    if(cin.GroupIndex == 0) // Avoid contention by other threads in the group.
    {
        GroupMinDepth = asuint(0.9999999f);
        GroupMaxDepth = 0;
        OpaqueLightCount = 0;
        GroupFrustum = Frustum[cin.GroupId.x * LightCulling.NumThreadGroupsX + cin.GroupId.x];
    }

    GroupMemoryBarrierWithGroupSync();

    InterlockedMin(GroupMinDepth, uDepth);
    InterlockedMax(GroupMaxDepth, uDepth);

    GroupMemoryBarrierWithGroupSync();

    float fMinDepth = asfloat(GroupMinDepth);
    float fMaxDepth = asfloat(GroupMaxDepth);

    float3 forward = Camera.Forward.xyz;
    float3 cameraPos = Camera.Position.xyz;

    float nearZ = DepthBufferToWorldSpaceDepth(fMinDepth, Camera.Near, Camera.Far, Camera.FarOverNear, Camera.OneMinusFarOverNear);
    float farZ = DepthBufferToWorldSpaceDepth(fMaxDepth, Camera.Near, Camera.Far, Camera.FarOverNear, Camera.OneMinusFarOverNear);

    LightCullingFrustumPlane cameraNearPlane = { forward, dot(forward, cameraPos + Camera.Near * forward)};
    LightCullingFrustumPlane nearPlane = { forward, dot(forward, cameraPos + nearZ * forward) };
    LightCullingFrustumPlane farPlane = { -forward, -dot(forward, cameraPos + farZ * forward) };

    uint numPointLights = LightCulling.NumPointLights;
    for(uint i = cin.GroupIndex; i < numPointLights; i += BLOCK_SIZE * BLOCK_SIZE) 
    {
        PointLightBoundingVolume pl = PointLights[i];
        if(PointLightInsideFrustum(pl, GroupFrustum, nearPlane, farPlane))
        {
            OpaqueAppendLight(i);
        }
    }

    // Todo: spotlights
    uint numSpotLights = 0;

    GroupMemoryBarrierWithGroupSync();

    if(cin.GroupIndex == 0)
    {
        InterlockedAdd(OpaqueLightIndexCounter[0], OpaqueLightCount, OpaqueLightIndexStartOffset);
        OpaqueLightGrid[cin.GroupId.xy] = uint2(OpaqueLightIndexStartOffset, OpaqueLightCount);
    }

    GroupMemoryBarrierWithGroupSync();

    for(i = cin.GroupIndex; i < OpaqueLightCount; i += BLOCK_SIZE * BLOCK_SIZE)
    {
        OpaqueLightIndexList[OpaqueLightIndexStartOffset + i] = OpaqueLightList[i];
    }
}