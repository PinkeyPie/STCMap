#include "../rs/default_pbr_rs.hlsli"
#include "../common/brdf.hlsli"
#include "../common/camera.hlsli"
#include "../rs/light_culling_rs.hlsli"
#include "../common/light_source.hlsli"
#include "../common/normal.hlsli"
#include "../common/material.hlsli"

struct PsInput
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;

	float4 screenPosition	: SV_POSITION;
};

ConstantBuffer<PbrMaterialCb> Material				: register(b0, space1);
ConstantBuffer<CameraCb> Camera						: register(b1, space1);
ConstantBuffer<LightingCb> Lighting					: register(b2, space1);


SamplerState WrapSampler								: register(s0);
SamplerState ClampSampler								: register(s1);
SamplerComparisonState ShadowSampler					: register(s2);


Texture2D<float4> AlbedoTex								: register(t0, space1);
Texture2D<float3> NormalTex								: register(t1, space1);
Texture2D<float> RoughTex								: register(t2, space1);
Texture2D<float> MetalTex								: register(t3, space1);


ConstantBuffer<DirectionalLightCb> Sun					: register(b0, space2);

TextureCube<float4> IrradianceTexture					: register(t0, space2);
TextureCube<float4> EnvironmentTexture					: register(t1, space2);

Texture2D<float2> Brdf									: register(t2, space2);

Texture2D<uint4> TiledCullingGrid						: register(t3, space2);
StructuredBuffer<uint> TiledObjectsIndexList			: register(t4, space2);
StructuredBuffer<PointLightCb> PointLights				: register(t5, space2);
StructuredBuffer<SpotLightCb> SpotLights				: register(t6, space2);
StructuredBuffer<PbrDecalCb> Decals						: register(t7, space2);
Texture2D<float> ShadowMap								: register(t8, space2);
StructuredBuffer<PointShadowInfo> pointShadowInfos		: register(t9, space2);
StructuredBuffer<SpotShadowInfo> spotShadowInfos		: register(t10, space2);

Texture2D<float4> decalTextureAtlas                     : register(t11, space2);

struct PsOutput
{
	float4 HdrColor		: SV_Target0;

#ifndef TRANSPARENT
	float2 WorldNormal	: SV_Target1;
	float4 Reflectance	: SV_Target2;
#endif
};

[RootSignature(DEFAULT_PBR_RS)]
PsOutput main(PsInput pin) 
{
	uint flags = Material.GetFlags();

	SurfaceInfo surface;

	surface.Albedo = ((flags & USE_ALBEDO_TEXTURE)
		? AlbedoTex.Sample(WrapSampler, pin.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* Material.GetAlbedo();

	const float normalMapStrength = Material.GetNormalMapStrength();
	surface.N = (flags & USE_NORMAL_TEXTURE)
		? mul(float3(normalMapStrength, normalMapStrength, 1.f) * (NormalTex.Sample(WrapSampler, pin.uv).xyz * 2.f - 1.f), pin.tbn)
		: pin.tbn[2];
	surface.N = normalize(surface.N);

	surface.Roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? RoughTex.Sample(WrapSampler, pin.uv)
		: Material.GetRoughnessOverride();
	surface.Roughness = clamp(surface.Roughness, 0.01f, 0.99f);

	surface.Metallic = (flags & USE_METALLIC_TEXTURE)
		? MetalTex.Sample(WrapSampler, pin.uv)
		: Material.GetMetallicOverride();

	surface.Emission = Material.Emission;

	surface.P = pin.worldPosition;
	float3 camToP = surface.P - Camera.Position.xyz;
	surface.V = -normalize(camToP);

	float pixelDepth = dot(Camera.Forward.xyz, camToP);

	LightContribution totalLighting = {float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f)};

	// Tiled lighting.
	const uint2 tileIndex = uint2(pin.screenPosition.xy / LIGHT_CULLING_TILE_SIZE);

#ifndef TRANSPARENT
	const uint2 tiledIndexData = TiledCullingGrid[tileIndex].xy;
#else
	const uint2 tiledIndexData = TiledCullingGrid[tileIndex].zw;
#endif

	const uint pointLightCount = (tiledIndexData.y >> 20) & 0x3FF;
	const uint spotLightCount = (tiledIndexData.y >> 10) & 0x3FF;
	const uint decalReadOffset = tiledIndexData.x;
	uint lightReadIndex = tiledIndexData.x + TILE_LIGHT_OFFSET;

	// Decals.
	float3 decalAlbedoAccum = (float3)0.f;
	float decalRoughnessAccum = 0.f;
	float decalMetallicAccum = 0.f;
	float decalAlphaAccum = 0.f;

	for (uint decalBucketIndex = 0; (decalBucketIndex < NUM_DECAL_BUCKETS) && (decalAlphaAccum < 1.f); ++decalBucketIndex)
	{
		uint bucket = TiledObjectsIndexList[decalReadOffset + decalBucketIndex];

		[loop]
		while(bucket) 
		{
			const uint indexOfLowestSetBit = firstbitlow(bucket);
			bucket ^= 1 << indexOfLowestSetBit; // Unset this bit.

			uint decalIndex = decalBucketIndex * 32 + indexOfLowestSetBit;
			decalIndex = MAX_NUM_TOTAL_DECALS - decalIndex - 1; // Reverse of operation in culling shader.
			PbrDecalCb decal = Decals[decalIndex];

			float3 offset = surface.P - decal.Position;
			float3 local = float3(
				dot(decal.Right, offset) / (dot(decal.Right, decal.Right)),
				dot(decal.Up, offset) / (dot(decal.Up, decal.Up)),
				dot(decal.Forward, offset) / (dot(decal.Forward, decal.Forward))
				);

			float decalStrength = saturate(dot(-surface.N, normalize(decal.Forward)));

			[branch]
			if(all(and(local >= -1.f, local <= 1.f)) && decalStrength > 0.f)
			{
				float2 uv = local.xy * 0.5f + 0.5f;                
				
				float4 viewport = decal.GetViewport();
				uv = viewport.xy + uv * viewport.zw;

				// Since this loop has variable length, we cannot use automatic mip-selection here. Gradients may be undefined.
				const float4 decalAlbedo = decalTextureAtlas.SampleLevel(WrapSampler, uv, 0) * decal.GetAlbedo();
				const float decalRoughness = decal.GetRoughnessOverride();
				const float decalMetallic = decal.GetMetallicOverride();
				
				const float alpha = decalAlbedo.a * decalStrength;
				const float oneMinusDecalAlphaAccum = 1.f - decalAlphaAccum;

				decalAlbedoAccum += oneMinusDecalAlphaAccum * (alpha * decalAlbedo.rgb);
				decalRoughnessAccum += oneMinusDecalAlphaAccum * (alpha * decalRoughness);
				decalMetallicAccum += oneMinusDecalAlphaAccum * (alpha * decalMetallic);

				decalAlphaAccum = alpha + (1.f - alpha) * decalAlphaAccum;

				[branch]
				if(decalAlphaAccum >= 1.f)
				{
					decalAlphaAccum = 1.f;
					break;
				}
			}
		}
	}

	surface.Albedo.rgb = lerp(surface.Albedo.rgb, decalAlbedoAccum, decalAlphaAccum);
	surface.Roughness = lerp(surface.Roughness, decalRoughnessAccum, decalAlphaAccum);
	surface.Metallic = lerp(surface.Metallic, decalMetallicAccum, decalAlphaAccum);

	surface.InferRemainingProperties();

	uint i;

	// Point lights.
	for (i = 0; i < pointLightCount; ++i)
	{
		PointLightCb pl = PointLights[TiledObjectsIndexList[lightReadIndex++]];

		LightInfo light;
		light.InitializeFromPointLight(surface, pl);

		float visibility = 1.f;

		[branch]
		if (pl.ShadowInfoIndex != -1)
		{
			PointShadowInfo info = pointShadowInfos[pl.ShadowInfoIndex];
			
			visibility = SamplePointLightShadowMapPCF(surface.P, pl.Position,
				ShadowMap,
				info.Viewport0, info.Viewport1,
				ShadowSampler,
				Lighting.ShadowMapTexelSize, pl.Radius);
		}

		[branch]
		if (visibility > 0.f)
		{
			totalLighting.Add(CalculateDirectLighting(surface, light), visibility);
		}
	}

	// Spot lights.
	for (i = 0; i < spotLightCount; ++i)
	{
		SpotLightCb sl = SpotLights[TiledObjectsIndexList[lightReadIndex++]];

		LightInfo light;
		light.InitializeFromSpotLight(surface, sl);

		float visibility = 1.f;

		[branch]
		if (sl.ShadowInfoIndex != -1)
		{
			SpotShadowInfo info = spotShadowInfos[sl.ShadowInfoIndex];
			visibility = SampleShadowMapPCF(info.ViewProj, surface.P,
				ShadowMap, info.Viewport,
				ShadowSampler,
				Lighting.ShadowMapTexelSize, info.Bias);
		}

		[branch]
		if (visibility > 0.f)
		{
			totalLighting.Add(CalculateDirectLighting(surface, light), visibility);
		}
	}

	// Ambient light.
	AmbientFactors factors = GetAmbientFactors(surface);
	totalLighting.Diffuse += DiffuseIBL(factors.Kd, surface, IrradianceTexture, ClampSampler) * Lighting.EnvironmentIntensity;

#ifdef TRANSPARENT
	// Only add ambient specular for transparent objects. Opaque objects get their ambient specular in a later render pass.
	totalLighting.Specular += SpecularIBL(factors.Ks, surface, EnvironmentTexture, Brdf, ClampSampler) * Lighting.EnvironmentIntensity;
#endif

	// Output.
	PsOutput pout;

#ifndef TRANSPARENT
	pout.HdrColor = totalLighting.Evaluate(surface.Albedo);
	pout.HdrColor.rgb += surface.Emission;

	pout.WorldNormal = PackNormal(surface.N);
	pout.Reflectance = float4(factors.Ks, surface.Roughness * (1.f - surface.N.y)); // Temporary: Up-facing surfaces get more reflective.
#else
	// Alpha-blending performs the following operation: final = alpha * src + (1 - alpha) * dest.
	// The factor on the destination-color is correct, and so is the factor on the diffuse part of the source-color.
	// However emission and specular light should not be modulated by alpha, since these components are not affected by transparency.
	// To counteract the hardware alpha-blending for these components, we pre-divide them by alpha.
	pout.HdrColor.rgb = (totalLighting.Specular + surface.Emission) * (1.f / max(surface.Albedo.a, 1e-5f));
	pout.HdrColor.rgb += totalLighting.Diffuse * surface.Albedo.rgb;
	pout.HdrColor.a = surface.Albedo.a;

	// Normal and reflectance are not needed for transparent surfaces.
#endif

	return pout;
}