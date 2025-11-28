#ifndef RAYTRACING_H
#define RAYTRACING_H

#include "common.hlsli"

struct RaytracingCb
{
    uint32 MaxRecursionDepth;
    float FadoutDistance;
    float MaxRayDistance;
    float EnvironmentIntensity;
    float SkyIntensity;
};

struct PathTracingCb
{
    uint32 FrameCount;
    uint32 NumAccumulatedFrames;
    uint32 MaxRecursionDepth;
    uint32 StartRussianRouletteAfter;

    uint32 UseThinLensCamera;
    float FocalLength;
    float LensRaduis;

    uint32 UseRealMaterials;
    uint32 EnableDirectLighting;
    float LightIntensityScale;
    float PointLightRadius;

    uint32 MultipleImportaceSampling;
};

#ifdef HLSL

static float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

// Careful: These two functions transform from BLAS space to TLAS space (world space). 
// However, if your geometry _inside_ the BLAS has a local transform, this is NOT
// accounted for here. For some reason, DX requires you to pass this transform in as 
// a buffer and do the transformation yourself, even though it has this information 
// already. 
static float3 TransformPositionToWorld(float3 position)
{
    float3x4 M = ObjectToWorld3x4();
    return mul(M, float4(position, 1.f)).xyz;
}

static float3 TransformDirectionToWorld(float3 direction)
{
	float3x4 M = ObjectToWorld3x4();
	return normalize(mul(M, float4(direction, 0.f)).xyz);
}

static float2 InterpolateAttribute(float2 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attribs)
{
	return vertexAttribute[0] +
		attribs.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attribs.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

static float3 InterpolateAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attribs)
{
	return vertexAttribute[0] +
		attribs.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attribs.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

static float4 InterpolateAttribute(float4 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attribs)
{
	return vertexAttribute[0] +
		attribs.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attribs.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

static uint3 Load3x16BitIndices(ByteAddressBuffer meshIndices)
{
	const uint indexSizeInBytes = 2;
	const uint indicesPerTriangle = 3;
	const uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	uint3 indices;

	// ByteAdressBuffer loads must be aligned at a 4 byte boundary.
	// Since we need to read three 16 bit indices: { 0, 1, 2 } 
	// aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
	// we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
	// based on first index's baseIndex being aligned at the 4 byte boundary or not:
	//  Aligned:     { 0 1 | 2 - }
	//  Not aligned: { - 0 | 1 2 }
	const uint dwordAlignedOffset = baseIndex & ~3;
	const uint2 four16BitIndices = meshIndices.Load2(dwordAlignedOffset);

	// Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
	if (dwordAlignedOffset == baseIndex)
	{
		indices.x = four16BitIndices.x & 0xffff;
		indices.y = (four16BitIndices.x >> 16) & 0xffff;
		indices.z = four16BitIndices.y & 0xffff;
	}
	else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
	{
		indices.x = (four16BitIndices.x >> 16) & 0xffff;
		indices.y = four16BitIndices.y & 0xffff;
		indices.z = (four16BitIndices.y >> 16) & 0xffff;
	}

	return indices;
}

static uint3 Load3x32BitIndices(ByteAddressBuffer meshIndices)
{
	const uint indexSizeInBytes = 4;
	const uint indicesPerTriangle = 3;
	const uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	uint3 indices = meshIndices.Load3(baseIndex);

	return indices;
}

#endif

#endif