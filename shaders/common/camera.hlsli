#ifndef CAMERA_HLSLI
#define CAMERA_HLSLI

#include "common.hlsli"

struct CameraCb
{
    mat4  View;
    mat4  Proj;
    mat4  ViewProj;
    mat4  InvView;
    mat4  InvProj;
    mat4  InvViewProj;
    
    vec4  Position;
    vec4  Forward;

    float Near;
    float Far;
    float FarOverNear;
    float OneMinusFarOverNear;

    vec2  ScreenDims;
    vec2  InvScreenDims;
};

static float3 RestoreViewSpacePosition(float2 uv, float depth, float4x4 invProj)
{
    uv.y = 1.f - uv.y; // Flip Y for UV coords
    float4 clipSpacePosition;
    clipSpacePosition.xy = uv * 2.f - 1.f;
    clipSpacePosition.z = depth;
    clipSpacePosition.w = 1.f;
    
    float4 viewSpacePosition = mul(invProj, clipSpacePosition);
    viewSpacePosition /= viewSpacePosition.w;
    
    return viewSpacePosition.xyz;
}

static float3 RestoreWorldSpacePosition(float2 uv, float depth, float4x4 invViewProj)
{
    uv.y = 1.f - uv.y; // Flip Y for UV coords
    float4 clipSpacePosition;
    clipSpacePosition.xy = uv * 2.f - 1.f;
    clipSpacePosition.z = depth;
    clipSpacePosition.w = 1.f;
    
    float4 worldSpacePosition = mul(invViewProj, clipSpacePosition);
    worldSpacePosition /= worldSpacePosition.w;
    
    return worldSpacePosition.xyz;
}

static float3 RestoreViewDirection(float2 uv, float4x4 invProj)
{
    return normalize(RestoreViewSpacePosition(uv, 1.f, invProj));
}

static float3 RestoreWorldDirection(float2 uv, float4x4 invViewProj, float3 cameraPosition, float farPlane)
{
    float3 direction = RestoreWorldSpacePosition(uv, 1.f, invViewProj) - cameraPosition; // At this point, the result should be on a plane 'farPlane' units away from the camera.
    direction /= farPlane;
    return normalize(direction);
}

static float DepthBufferDepthToLinearNormalizedDepthEyeToFarPlane(float depthBufferDepth, float farOverNear, float oneMinusFarOverNear)
{
    return 1.f / (depthBufferDepth * oneMinusFarOverNear + farOverNear);
}

static float DepthBufferDepthToLinearWorldDepthEyeToFarPlane(float depthBufferDepth, float farOverNear, float oneMinusFarOverNear, float far)
{
    return DepthBufferDepthToLinearNormalizedDepthEyeToFarPlane(depthBufferDepth, farOverNear, oneMinusFarOverNear) * far; // near
}

struct CameraFrustumPlanes 
{
    float4 planes[6]; // left, right, bottom, top, near, far
};

static bool CullWorldSpaceAABB(CameraFrustumPlanes frustum, float4 aabbMin, float4 aabbMax) 
{
    for (int i = 0; i < 6; ++i) 
    {
        float4 plane = frustum.planes[i];
        float3 positiveVertex = float4(
            plane.x >= 0 ? aabbMax.x : aabbMin.x,
            plane.y >= 0 ? aabbMax.y : aabbMin.y,
            plane.z >= 0 ? aabbMax.z : aabbMin.z,
            1.f
        );
        
        if (dot(plane.xyz, positiveVertex) < 0.f) 
        {
            return true; // Cull
        }
    }
    return false; // Not culled
}

static bool CullModelSpaceAABB(CameraFrustumPlanes planes, float4 minAabb, float4 maxAabb, float4x4 transform) 
{
    float4 worldSpaceCorners[] = {
        mul(transform, float4(minAabb.x, minAabb.y, minAabb.z, 1.f)),
        mul(transform, float4(maxAabb.x, minAabb.y, minAabb.z, 1.f)),
        mul(transform, float4(minAabb.x, maxAabb.y, minAabb.z, 1.f)),
        mul(transform, float4(maxAabb.x, maxAabb.y, minAabb.z, 1.f)),
        mul(transform, float4(minAabb.x, minAabb.y, maxAabb.z, 1.f)),
        mul(transform, float4(maxAabb.x, minAabb.y, maxAabb.z, 1.f)),
        mul(transform, float4(minAabb.x, maxAabb.y, maxAabb.z, 1.f)),
        mul(transform, float4(maxAabb.x, maxAabb.y, maxAabb.z, 1.f))
    };

    for(uint i = 0; i < 6; ++i) 
    {
        float4 plane = planes.planes[i];
        bool allOutside = true;
        
        for (int j = 0; j < 8; ++j) 
        {
            if (dot(plane.xyz, worldSpaceCorners[j].xyz) >= 0.f) 
            {
                allOutside = false;
                break;
            }
        }
        
        if (allOutside) 
        {
            return true; // Cull
        }
    }

    return false; // Not culled
}

#endif