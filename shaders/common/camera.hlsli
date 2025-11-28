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
    mat4 PrevFrameViewProj;

    vec4  Position;
    vec4  Forward;
    vec4  Right;
    vec4  Up;

    vec4 ProjectionParams; // nearPlane, farPlane, farPlane / nearPlane, 1 - farPlane / nearPlane

    vec2  ScreenDims;
    vec2  InvScreenDims;
    vec2  Jitter;
    vec2  PrevFrameJitter;
};

#ifdef HLSL
static float3 RestoreViewSpacePosition(float2 uv, float depth, float4x4 invProj)
{
    uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	float3 ndc = float3(uv * 2.f - float2(1.f, 1.f), depth);
	float4 homPosition = mul(invProj, float4(ndc, 1.f));
	float3 position = homPosition.xyz / homPosition.w;
	return position;
}

static float3 RestoreWorldSpacePosition(float2 uv, float depth, float4x4 invViewProj)
{
    uv.y = 1.f - uv.y; // Screen uvs start at the top left, so flip y.
	float3 ndc = float3(uv * 2.f - float2(1.f, 1.f), depth);
	float4 homPosition = mul(invViewProj, float4(ndc, 1.f));
	float3 position = homPosition.xyz / homPosition.w;
	return position;
}

// The directions are NOT normalized. Their z-coordinate is 'nearPlane' long.
static float3 RestoreViewDirection(float2 uv, float4x4 invProj)
{
    return RestoreViewSpacePosition(uv, 0.f, invProj);
}

static float3 RestoreWorldDirection(float2 uv, float4x4 invViewProj, float3 cameraPosition)
{
    return RestoreWorldSpacePosition(uv, 0.f, invViewProj) - cameraPosition; // At this point, the result should be 'nearPlane' units away from the camera.
}

// This function returns a positive z value! This is a depth!
static float DepthBufferDepthToEyeDepth(float depthBufferDepth, float4 projectionParams)
{
	if (projectionParams.y < 0.f) // Infinite far plane.
	{
		depthBufferDepth = clamp(depthBufferDepth, 0.f, 1.f - 1e-7f); // A depth of 1 is at infinity.
		return -projectionParams.x / (depthBufferDepth - 1.f);
	}
	else
	{
		const float c1 = projectionParams.z;
		const float c0 = projectionParams.w;
		return projectionParams.y / (c0 * depthBufferDepth + c1);
	}
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
        float4 positiveVertex = float4(
            plane.x >= 0 ? aabbMax.x : aabbMin.x,
            plane.y >= 0 ? aabbMax.y : aabbMin.y,
            plane.z >= 0 ? aabbMax.z : aabbMin.z,
            1.f
        );
        
        if (dot(plane, positiveVertex) < 0.f) 
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

#endif