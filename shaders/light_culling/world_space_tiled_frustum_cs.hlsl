#include "../common/cs.hlsli"
#include "../common/camera.hlsli"
#include "../rs/light_culling_rs.hlsli"

#define BLOCK_SIZE 16

ConstantBuffer<CameraCb> Camera   : register(b0);
ConstantBuffer<FrustumCb> Frustum : register(b1);
RWStructuredBuffer<LightCullingViewFrustum> OutFrustum : register(u0);

static LightCullingFrustumPlane GetPlane(float3 p0, float3 p1, float3 p2) 
{
    // The plane normal points to the inside of the frustum.

    LightCullingFrustumPlane plane;

    float3 v0 = p1 - p0;
    float3 v2 = p2 - p0;

    plane.N = normalize(cross(v0, v2));
    plane.d = dot(plane.N, p0);

    return plane;
}

[RootSignature(WORLD_SPACE_TILED_FRUSTUM_RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput cin)
{
    float2 invScreenDims = Camera.InvScreenDims;

    float2 screenTL = invScreenDims * (cin.DispatchThreadId.xy * LIGHT_CULLING_TILE_SIZE);
    float2 screenTR = invScreenDims * (float2(cin.DispatchThreadId.x + 1, cin.DispatchThreadId.y) * LIGHT_CULLING_TILE_SIZE);
    float2 screenBL = invScreenDims * (float2(cin.DispatchThreadId.x, cin.DispatchThreadId.y + 1) * LIGHT_CULLING_TILE_SIZE);
    float2 screenBR = invScreenDims * (float2(cin.DispatchThreadId.x + 1, cin.DispatchThreadId.y + 1) * LIGHT_CULLING_TILE_SIZE);

    // Points on near plane.
    float3 tl = RestoreWorldSpacePosition(screenTL, 0.f, Camera.InvViewProj);
    float3 tr = RestoreWorldSpacePosition(screenTR, 0.f, Camera.InvViewProj);
    float3 bl = RestoreWorldSpacePosition(screenBL, 0.f, Camera.InvViewProj);
    float3 br = RestoreWorldSpacePosition(screenBR, 0.f, Camera.InvViewProj);

    float3 cameraPos = Camera.Position.xyz;
    LightCullingViewFrustum frustum;
    frustum.Planes[0] = GetPlane(cameraPos, bl, tl);
    frustum.Planes[1] = GetPlane(cameraPos, tr, br);
    frustum.Planes[2] = GetPlane(cameraPos, tl, tr);
    frustum.Planes[3] = GetPlane(cameraPos, br, bl);

    if (cin.DispatchThreadId.x < Frustum.NumThreadsX && cin.DispatchThreadId.y < Frustum.NumThreadsY)
    {
        uint index = cin.DispatchThreadId.y * Frustum.NumThreadsX + cin.DispatchThreadId.x;
        OutFrustum[index] = frustum;
    }
}