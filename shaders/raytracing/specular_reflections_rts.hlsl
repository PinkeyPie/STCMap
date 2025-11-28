#include "../common/camera.hlsli"
#include "../common/raytracing.hlsli"
#include "../common/light_source.hlsli"
#include "../common/brdf.hlsli"
#include "../common/normal.hlsli"
#include "../common/material.hlsli"

// Raytracing intrinsics: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-system-values
// Ray flags: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-flags

struct MeshVertex
{
	float3 Position;
	float2 UV;
	float3 Normal;
	float3 Tangent;
};


RWTexture2D<float4> Output					: register(u0);

RaytracingAccelerationStructure RtScene		: register(t0);
Texture2D<float> DepthBuffer                : register(t1);
Texture2D<float2> WorldNormals				: register(t2);
TextureCube<float4> IrradianceTexture		: register(t3);
TextureCube<float4> EnvironmentTexture		: register(t4);
TextureCube<float4> Sky						: register(t5);
Texture2D<float4> Brdf						: register(t6);


ConstantBuffer<CameraCb> Camera			: register(b0);
ConstantBuffer<DirectionalLightCb> Sun	: register(b1);
ConstantBuffer<RaytracingCb> Raytracing	: register(b2);
SamplerState WrapSampler					: register(s0);
SamplerState ClampSampler					: register(s1);


// Radiance hit group.
ConstantBuffer<PbrMaterialCb> Material	    : register(b0, space1);
StructuredBuffer<MeshVertex> MeshVertices	: register(t0, space1);
ByteAddressBuffer MeshIndices				: register(t1, space1);
Texture2D<float4> AlbedoTex					: register(t2, space1);
Texture2D<float3> NormalTex					: register(t3, space1);
Texture2D<float> RoughTex					: register(t4, space1);
Texture2D<float> MetalTex					: register(t5, space1);


struct RadianceRayPayload
{
	float3 Color;
	uint Recursion;
};

struct ShadowRayPayload
{
	bool HitGeometry;
};

static float3 SampleEnvironment(float3 direction)
{
	return Sky.SampleLevel(WrapSampler, direction, 0).xyz * Raytracing.SkyIntensity;
}

static float3 traceRadianceRay(float3 origin, float3 direction, uint recursion)
{
	if (recursion >= Raytracing.MaxRecursionDepth)
	{
		return float3(0, 0, 0);
	}

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = Raytracing.MaxRayDistance;

	RadianceRayPayload payload = { float3(0.f, 0.f, 0.f), recursion + 1 };

	TraceRay(RtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,				// Cull mask.
		RADIANCE,			// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		RADIANCE,			// Miss index.
		ray,
		payload);

	return payload.Color;
}

static bool TraceShadowRay(float3 origin, float3 direction, uint recursion)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 100000.f;

	ShadowRayPayload payload = { true };

	TraceRay(RtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE,
		0xFF,				// Cull mask.
		SHADOW,				// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		SHADOW,				// Miss index.
		ray,
		payload);

	return payload.HitGeometry;
}

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float2 uv = float2(launchIndex.xy) / float2(launchDim.xy);
	float4 depth4 = DepthBuffer.Gather(ClampSampler, uv);
	uint minComponent =
		(depth4.x < depth4.y) ?
		(depth4.x < depth4.z) ?
		(depth4.x < depth4.w) ? 0 : 3 :
		(depth4.z < depth4.w) ? 2 : 3 :
		(depth4.y < depth4.z) ?
		(depth4.y < depth4.w) ? 1 : 3 :
		(depth4.z < depth4.w) ? 2 : 3;

	float depth = depth4[minComponent];

	float3 color = (float3)0.f;
	if (depth < 1.f)
	{
#if 1
		float3 origin = RestoreWorldSpacePosition(uv, depth, Camera.InvViewProj);
		float3 direction = normalize(origin - Camera.Position);

		float2 normal2 = float2(
			WorldNormals.GatherRed(ClampSampler, uv)[minComponent],
			WorldNormals.GatherGreen(ClampSampler, uv)[minComponent]
		);
		float3 normal = UnpackNormal(normal2);
		direction = reflect(direction, normal);
#else
		float3 origin = camera.position.xyz;
		float3 direction = normalize(restoreWorldDirection(camera.invViewProj, uv, origin));
#endif

		color = traceRadianceRay(origin, direction, 0);
	}
	Output[launchIndex.xy] = float4(color, 1);
}

// ----------------------------------------
// RADIANCE
// ----------------------------------------

[shader("miss")]
void radianceMiss(inout RadianceRayPayload payload)
{
	payload.Color = SampleEnvironment(WorldRayDirection());
}

[shader("closesthit")]
void radianceClosestHit(inout RadianceRayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint3 tri = Load3x16BitIndices(MeshIndices);

	float2 uvs[] = { MeshVertices[tri.x].UV, MeshVertices[tri.y].UV, MeshVertices[tri.z].UV };
	float3 normals[] = { MeshVertices[tri.x].Normal, MeshVertices[tri.y].Normal, MeshVertices[tri.z].Normal };

	float2 uv = InterpolateAttribute(uvs, attribs);
    float3 attributes = InterpolateAttribute(normals, attribs);
	float3 N = normalize(TransformDirectionToWorld(attributes));

	uint mipLevel = 0;


	uint flags = Material.GetFlags();

	float4 albedo = ((flags & USE_ALBEDO_TEXTURE)
		? AlbedoTex.SampleLevel(WrapSampler, uv, mipLevel)
		: float4(1.f, 1.f, 1.f, 1.f))
		* UnpackColor(Material.AlbedoTint);

	// We ignore normal maps for now.

	float roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? RoughTex.SampleLevel(WrapSampler, uv, mipLevel)
		: Material.GetRoughnessOverride();
	roughness = clamp(roughness, 0.01f, 0.99f);

	float metallic = (flags & USE_METALLIC_TEXTURE)
		? MetalTex.SampleLevel(WrapSampler, uv, mipLevel)
		: Material.GetMetallicOverride();

	float ao = 1.f;// (flags & USE_AO_TEXTURE) ? RMAO.z : 1.f;



	float3 hitPosition = HitWorldPosition();
	float3 L = -Sun.Direction;
	float visibility = 1.f - (float)TraceShadowRay(hitPosition, L, payload.Recursion);
	
	float3 radiance = Sun.Radiance * visibility; // No attenuation for sun.
	float3 V = -WorldRayDirection();
	float3 R0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	
	payload.Color = CalculateDirectLighting(albedo, radiance, N, L, V, R0, roughness, metallic);
	payload.Color += CalculateAmbientLight(albedo, IrradianceTexture, EnvironmentTexture, Brdf, ClampSampler, N, V, R0, roughness, metallic, ao) * Raytracing.EnvironmentIntensity;

	float3 reflectionDirection = normalize(reflect(WorldRayDirection(), N));
	float3 bounceRadiance = traceRadianceRay(hitPosition, reflectionDirection, payload.Recursion);
	payload.Color += CalculateDirectLighting(albedo, bounceRadiance, N, reflectionDirection, V, R0, roughness, metallic);

	float t = RayTCurrent();
	if (t > Raytracing.FadoutDistance)
	{
		float3 env = SampleEnvironment(WorldRayDirection());
		payload.Color = lerp(payload.Color, env, (t - Raytracing.FadoutDistance) / (Raytracing.MaxRayDistance - Raytracing.FadoutDistance));
	}
}

[shader("anyhit")]
void radianceAnyHit(inout RadianceRayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	AcceptHitAndEndSearch();
}

// ----------------------------------------
// SHADOW
// ----------------------------------------
[shader("miss")]
void shadowMiss(inout ShadowRayPayload payload)
{
	payload.HitGeometry = false;
}

[shader("closesthit")]
void shadowClosestHit(inout ShadowRayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.HitGeometry = true;
}

[shader("anyhit")]
void shadowAnyHit(inout ShadowRayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	AcceptHitAndEndSearch();
}
