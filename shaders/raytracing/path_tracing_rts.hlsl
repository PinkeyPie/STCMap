#include "../common/camera.hlsli"
#include "../common/raytracing.hlsli"
#include "../common/brdf.hlsli"
#include "../common/material.hlsli"
#include "../common/random.hlsli"
#include "../common/light_source.hlsli"

// Raytracing intrinsics: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-system-values
// Ray flags: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-flags

struct MeshVertex
{
	float3 Position;
	float2 UV;
	float3 Normal;
	float3 Tangent;
};


// Global.
RaytracingAccelerationStructure RtScene		: register(t0);
TextureCube<float4> Sky						: register(t1);

RWTexture2D<float4> Output					: register(u0);

ConstantBuffer<CameraCb> Camera			    : register(b0);
ConstantBuffer<PathTracingCb> Constants	    : register(b1);

SamplerState WrapSampler					: register(s0);


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
	uint RandSeed;
};

struct ShadowRayPayload
{
	float Visible;
};


#define NUM_LIGHTS 3

static const PointLightCb pointLights[NUM_LIGHTS] =
{
	// Position, radius, radiance. The last value (-1) is only useful for rasterization, where this is the index into a list of shadow maps (-1 means no shadows).
	{
		float3(0.f, 3.f, 0.f),
		15.f,
		float3(0.8f, 0.2f, 0.1f) * 50.f,
		-1
	},
	{
		float3(-5.f, 8.f, 0.f),
		15.f,
		float3(0.2f, 0.8f, 0.3f) * 50.f,
		-1
	},
	{
		float3(5.f, 8.f, 0.f),
		15.f,
		float3(0.2f, 0.3f, 0.8f) * 50.f,
		-1
	},
};



static float3 TraceRadianceRay(float3 origin, float3 direction, uint randSeed, uint recursion)
{
	// This is replaced by the russian roulette below.
	/*if (recursion >= constants.maxRecursionDepth)
	{
		return float3(0, 0, 0);
	}*/

	// My attempt at writing a russian roulette termination, which guarantees that the recursion depth does not exceed the maximum.
	// Lower numbers make rays terminate earlier, which improves performance, but hurts the convergence speed.
	// I think normally you wouldn't want the termination probability to go to 1, but DirectX will remove the device, if you exceed
	// the recursion limit.
	float russianRouletteFactor = 1.f;
	if (recursion >= Constants.StartRussianRouletteAfter)
	{
		uint rouletteSteps = Constants.MaxRecursionDepth - Constants.StartRussianRouletteAfter + 1;
		uint stepsRemaining = recursion - Constants.StartRussianRouletteAfter + 1;
		float stopProbability = min(1.f, (float)stepsRemaining / (float)rouletteSteps);

		if (NextRand(randSeed) <= stopProbability)
		{
			return (float3)0.f;
		}

		russianRouletteFactor = 1.f / (1.f - stopProbability);
	}

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 10000.f;

	RadianceRayPayload payload = { float3(0.f, 0.f, 0.f), recursion + 1, randSeed };

	TraceRay(RtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,				// Cull mask.
		RADIANCE,			// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		RADIANCE,			// Miss index.
		ray,
		payload);

	return payload.Color * russianRouletteFactor;
}

static float traceShadowRay(float3 origin, float3 direction, float distance, uint recursion) // This shader type is also used for ambient occlusion. Just set the distance to something small.
{
	if (recursion >= Constants.MaxRecursionDepth)
	{
		return 1.f;
	}

#ifdef SHADOW
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = distance;

	ShadowRayPayload payload = { 0.f };

	TraceRay(RtScene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // No need to invoke closest hit shader.
		0xFF,				// Cull mask.
		SHADOW,				// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		SHADOW,				// Miss index.
		ray,
		payload);

	return payload.Visible;
#else
	return 1.f;
#endif
}





// ----------------------------------------
// RAY GENERATION
// ----------------------------------------

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, Constants.FrameCount, 16);


	// Jitter for anti-aliasing.
	float2 pixelOffset = float2(NextRand(randSeed), NextRand(randSeed));
	float2 uv = (float2(launchIndex.xy) + pixelOffset) / float2(launchDim.xy);

	float3 origin = Camera.Position.xyz;
	float3 direction = RestoreWorldDirection(uv, Camera.InvViewProj, origin);


	if (Constants.UseThinLensCamera)
	{
		direction /= Camera.ProjectionParams.x;

		float3 focalPoint = origin + Constants.FocalLength * direction;

		float2 rnd = float2(2.f * PI * NextRand(randSeed), Constants.LensRaduis * NextRand(randSeed));
		float2 originOffset = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

		origin += Camera.Right.xyz * originOffset.x + Camera.Up.xyz * originOffset.y;
		direction = focalPoint - origin;
	}

	direction = normalize(direction);


	// Trace ray.
	float3 color = TraceRadianceRay(origin, direction, randSeed, 0);


	// Blend result color with previous frames.
	float3 previousColor = Output[launchIndex.xy].xyz;
	float previousCount = (float)Constants.NumAccumulatedFrames;
	float3 newColor = (previousCount * previousColor + color) / (previousCount + 1);

	Output[launchIndex.xy] = float4(newColor, 1.f);
}




// ----------------------------------------
// RADIANCE
// ----------------------------------------

static float probabilityToSampleDiffuse(float roughness)
{
	// I don't know what a good way to choose is. My understanding is that it doesn't really matter, as long as you account for the probability in your results. 
	// It has an impact on covergence speed though.
	// TODO: Can we calculate this using fresnel?
	return 0.5f;
	return roughness;
}

static float3 calculateIndirectLighting(inout uint randSeed, SurfaceInfo surface, uint recursion)
{
	float probDiffuse = probabilityToSampleDiffuse(surface.Roughness);
	float chooseDiffuse = (NextRand(randSeed) < probDiffuse);

	if (chooseDiffuse)
	{
		float3 L = GetCosHemisphereSample(randSeed, surface.N);
		float3 bounceColor = TraceRadianceRay(surface.P, L, randSeed, recursion);

		// Accumulate the color: (NdotL * incomingLight * dif / pi) 
		// Probability of sampling:  (NdotL / pi) * probDiffuse
		return bounceColor * surface.Albedo.rgb / probDiffuse;
	}
	else
	{
		float3 H = ImportanceSampleGGX(randSeed, surface.N, surface.Roughness);
		float3 L = reflect(-surface.V, H);

		float3 bounceColor = TraceRadianceRay(surface.P, L, randSeed, recursion);

		float NdotV = saturate(dot(surface.N, surface.V));
		float NdotL = saturate(dot(surface.N, L));
		float NdotH = saturate(dot(surface.N, H));
		float LdotH = saturate(dot(L, H));

		float D = DistributionGGX(NdotH, surface.Roughness);
		float G = GeometrySmith(NdotL, NdotV, surface.Roughness);
		float3 F = FresnelSchlick(LdotH, surface.R0);

		float3 numerator = D * G * F;
		float denominator = 4.f * NdotV * NdotL;
		float3 brdf = numerator / max(denominator, 0.001f);

		// Probability of sampling vector H from GGX.
		float ggxProb = max(D * NdotH / (4.f * LdotH), 0.01f);

		// Accumulate the color:  ggx-BRDF * incomingLight * NdotL / probability-of-sampling
		//    -> Should really simplify the math above.
		return NdotL * bounceColor * brdf / (ggxProb * (1.f - probDiffuse));
	}
}


[shader("closesthit")]
void radianceClosestHit(inout RadianceRayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint3 tri = Load3x32BitIndices(MeshIndices); // Use load3x16BitIndices, if you have 16-bit indices.

	// Interpolate vertex attributes over triangle.
	float2 uvs[] = { MeshVertices[tri.x].UV, MeshVertices[tri.y].UV, MeshVertices[tri.z].UV };
	float3 normals[] = { MeshVertices[tri.x].Normal, MeshVertices[tri.y].Normal, MeshVertices[tri.z].Normal };

	float2 uv = InterpolateAttribute(uvs, attribs);

	SurfaceInfo surface;
    float3 attributes = InterpolateAttribute(normals, attribs);
	surface.N = normalize(TransformDirectionToWorld(attributes));
	surface.V = -WorldRayDirection();
	surface.P = HitWorldPosition();

	surface.Emission = (float3)0.f;
	surface.Albedo = (float4)1.f;
	surface.Roughness = 1.f;
	surface.Metallic = 0.f;

	if (Constants.UseRealMaterials)
	{
		uint mipLevel = 0;
		uint flags = Material.GetFlags();

		surface.Albedo = (((flags & USE_ALBEDO_TEXTURE)
			? AlbedoTex.SampleLevel(WrapSampler, uv, mipLevel)
			: float4(1.f, 1.f, 1.f, 1.f))
			* UnpackColor(Material.AlbedoTint));

		// We ignore normal maps for now.

		surface.Roughness = (flags & USE_ROUGHNESS_TEXTURE)
			? RoughTex.SampleLevel(WrapSampler, uv, mipLevel)
			: Material.GetRoughnessOverride();

		surface.Metallic = (flags & USE_METALLIC_TEXTURE)
			? MetalTex.SampleLevel(WrapSampler, uv, mipLevel)
			: Material.GetMetallicOverride();

		surface.Emission = Material.Emission;
	}

	surface.Roughness = clamp(surface.Roughness, 0.01f, 0.99f);

	surface.InferRemainingProperties();

	payload.Color = surface.Emission;
	payload.Color += calculateIndirectLighting(payload.RandSeed, surface, payload.Recursion);


	if (Constants.EnableDirectLighting)
	{
		// Sun light.
		{
			float3 sunL = -normalize(float3(-0.6f, -1.f, -0.3f));
			float3 sunRadiance = float3(1.f, 0.93f, 0.76f) * Constants.LightIntensityScale * 2.f;

			LightInfo light;
			light.Initialize(surface, sunL, sunRadiance);

			payload.Color +=
				CalculateDirectLighting(surface, light).Evaluate(surface.Albedo)
				* traceShadowRay(surface.P, sunL, 10000.f, payload.Recursion);
		}



		// Random point light.
		{
			uint lightIndex = min(uint(NUM_LIGHTS * NextRand(payload.RandSeed)), NUM_LIGHTS - 1);

			LightInfo light;
			light.InitializeFromPointOnSphereLight(surface, pointLights[lightIndex], Constants.PointLightRadius, payload.RandSeed);

			float pointLightVisibility = traceShadowRay(surface.P, light.L, light.DistanceToLight, payload.Recursion);

			float3 pointLightColor =
				CalculateDirectLighting(surface, light).Evaluate(surface.Albedo)
				* pointLightVisibility;

			float pointLightSolidAngle = SolidAngleOfSphere(Constants.PointLightRadius, light.DistanceToLight) * 0.5f; // Divide by 2, since we are only interested in hemisphere.
			if (Constants.MultipleImportaceSampling)
			{
				// Multiple importance sampling. At least if I've done this correctly. See http://www.cs.uu.nl/docs/vakken/magr/2015-2016/slides/lecture%2008%20-%20variance%20reduction.pdf, slide 50.
				float lightPDF = 1.f / (pointLightSolidAngle * NUM_LIGHTS); // Correct for PDFs, see comment below.

				// Lambertian part.
				float diffusePDF = dot(surface.N, light.L) * INV_PI; // Cosine-distributed for Lambertian BRDF.

				// Specular part.
				float D = DistributionGGX(surface, light);
				float specularPDF = max(D * light.NdotH / (4.f * light.LdotH), 0.01f);

				float probDiffuse = probabilityToSampleDiffuse(surface.Roughness);

				// Total BRDF PDF. This is the probability that we had randomly hit this direction using our brdf importance sampling.
				float brdfPDF = lerp(specularPDF, diffusePDF, probDiffuse);

				// Blend PDFs.
				float t = lightPDF / (lightPDF + brdfPDF);
				float misPDF = lerp(brdfPDF, lightPDF, t); // Balance heuristic.

				pointLightColor /= misPDF;
			}
			else
			{
				pointLightColor = pointLightColor
					* NUM_LIGHTS			 // Correct for probability of choosing this particular light.
					* pointLightSolidAngle;  // Correct for probability of "randomly" hitting this light. I *think* this is correct. See https://github.com/NVIDIA/Q2RTX/blob/master/src/refresh/vkpt/shader/light_lists.h#L295.
			}

			payload.Color += pointLightColor;
		}
	}
}

[shader("miss")]
void radianceMiss(inout RadianceRayPayload payload)
{
	payload.Color = Sky.SampleLevel(WrapSampler, WorldRayDirection(), 0).xyz;
}

// ----------------------------------------
// SHADOW
// ----------------------------------------
[shader("miss")]
void shadowMiss(inout ShadowRayPayload payload)
{
	payload.Visible = 1.f;
}
