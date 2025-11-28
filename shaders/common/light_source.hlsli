#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

// Used for point and spot lights, because I dislike very high numbers.
#define LIGHT_RADIANCE_SCALE 1000.f

#define MAX_NUM_SHADOW_CASCADES 4

#include "common.hlsli"

static float GetAttenuation(float distance, float maxDistance)
{
    // https://imdoingitwrong.wordpress.com/2011/02/10/improved-light-attenuation/
	float relDist = Min(distance / maxDistance, 1.f);
	float d = distance / (1.f - relDist * relDist);
	
	float att =  1.f / (d * d + 1.f);
	return att;
}

struct DirectionalLightCb
{
    mat4 ViewProj[MAX_NUM_SHADOW_CASCADES];
    vec4 Viewports[MAX_NUM_SHADOW_CASCADES];

    vec4 CascadeDistances;
    vec4 Bias;

    vec3 Direction;
    uint32 NumShadowCascades;
    vec3 Radiance;
    uint32 Padding;
    vec4 BlendDistances;
};

struct PointLightCb
{
    vec3 Position;
    float Radius;   // Maximum distance.
    vec3 Radiance;
    int ShadowInfoIndex; // -1, if light casts no shadows.

    void Initialize(vec3 position, vec3 radiance, float radius, int shadowInfoIndex = -1)
    {
        Position = position;
        Radiance = radiance;
        Radius = radius;
        ShadowInfoIndex = shadowInfoIndex;
    }

#ifndef HLSL
    PointLightCb() {}

    PointLightCb(vec3 position, vec3 radiance, float radius, int shadowInfoIndex = -1)
    {
        Initialize(position, radiance, radius, shadowInfoIndex);
    }
#endif
};

struct SpotLightCb
{
    vec3 Position;
    int InnerAndOuterCutoff; // cos(InnerAngle) << 16 | cos(outerAngle). Both are packed into 16 bit signed ints.
    vec3 Direction;
    float MaxDistance;
    vec3 Radiance;
    int ShadowInfoIndex; // -1, if light casts no shadows.

    void Initialize(vec3 position, vec3 direction, vec3 radiance, float innerAngle, float outerAngle, float maxDistance, int shadowInfoIndex = -1)
    {
        Position = position;
        Direction = direction;
        Radiance = radiance;

        int inner = (int)(cos(innerAngle) * ((1 << 15) - 1));
        int outer = (int)(cos(outerAngle) * ((1 << 15) - 1));
        InnerAndOuterCutoff = (inner << 16) | outer;

        MaxDistance = maxDistance;
        ShadowInfoIndex = shadowInfoIndex;
    }

#ifndef HLSL
    SpotLightCb() {}

    SpotLightCb(vec3 position, vec3 direction, vec3 radiance, float innerAngle, float outerAngle, float maxDistance, int shadowInfoIndex = -1)
    {
        Initialize(position, direction, radiance, innerAngle, outerAngle, maxDistance, shadowInfoIndex);
    }
#endif

    float GetInnerCutoff()
#ifndef HLSL
        const
#endif
    {
        return (InnerAndOuterCutoff >> 16) / float((1 << 15) - 1);
    }

    float GetOuterCutoff()
#ifndef HLSL
        const
#endif
    {
        return (InnerAndOuterCutoff & 0xffff) / (float(1 << 15) - 1);
    }
};

struct SpotShadowInfo
{
    mat4 ViewProj;
    vec4 Viewport;
    float Bias;
    float Padding0[3];
};

struct PointShadowInfo
{
    vec4 Viewport0;
    vec4 Viewport1;
};

#ifdef HLSL
static float SampleShadowMapSimple(float4x4 viewProj, float3 worldPos, Texture2D<float> shadowMap, float4 viewport, 
    SamplerComparisonState shadowMapSampler, float bias)
{
    float4 lightProjected = mul(viewProj, float4(worldPos, 1.f));
    lightProjected.xyz /= lightProjected.w;

    float visibility = 1.f;

    if(lightProjected.z < 1.f)
    {
        float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
        lightUV.y = 1.f - lightUV.y;

        lightUV = lightUV * viewport.zw + viewport.xy;

        visibility = shadowMap.SampleCmpLevelZero(
            shadowMapSampler,
            lightUV,
            lightProjected.z - bias
        );
    }
    return visibility;
}

static float SampleShadowMapPCF(float4x4 viewProj, float3 worldPos, Texture2D<float> shadowMap,
    float4 viewport, SamplerComparisonState shadowMapSampler, float2 texelSize, float bias)
{
    float4 lightProjected = mul(viewProj, float4(worldPos, 1.f));
    lightProjected.xyz /= lightProjected.w;

    float visibility = 1.f;

    if(lightProjected.z < 1.f)
    {
        visibility = 0.f;

        float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
        lightUV.y = 1.f - lightUV.y;

        lightUV = lightUV * viewport.zw + viewport.xy;

        for(float y = -1.5f; y <= 1.6f; y += 1.f)
        {
            for(float x = -1.5f; x <= 1.6f; x += 1.f)
            {
                visibility = shadowMap.SampleCmpLevelZero(
                    shadowMapSampler,
                    lightUV + float2(x, y) * texelSize,
                    lightProjected.z - bias
                );
            }
        }
        visibility /= 16.f;
    }
    return visibility;
}

static float SamplePointLightShadowMapPCF(float3 worldPos, float3 lightPos, Texture2D<float> shadowMap,
    vec4 viewport, vec4 viewport2, SamplerComparisonState shadowMapSampler, float2 texelSize, float maxDistance)
{
    float3 L = worldPos - lightPos;
    float l = length(L);
    L /= l;

    float flip = L.z > 0.f ? 1.f : -1.f;
    vec4 viewProj = L.z > 0.f ? viewport : viewport2;

    L.z *= flip;
    L.xy /= L.z + 1.f;

    float2 lightUV = L.xy * 0.5f + float2(0.5f, 0.5f);
    lightUV.y = 1.f - lightUV.y;

    lightUV = lightUV * viewProj.zw + viewProj.xy;

    float compareDistance = l / maxDistance;

    float bias = 0.001f * flip;

    float visibility = 0.f;
    for (float y = -1.5f; y <= 1.6f; y += 1.f)
	{
		for (float x = -1.5f; x <= 1.6f; x += 1.f)
		{
			visibility += shadowMap.SampleCmpLevelZero(
				shadowMapSampler,
				lightUV + float2(x, y) * texelSize,
				compareDistance - bias
            );
		}
	}
	visibility /= 16.f;

    return visibility;
}

static float SampleCascadedShadowMapSimple(float4x4 viewProj[4], float3 worldPosition,
    Texture2D<float> shadowMap, float4 viewports[4], SamplerComparisonState shadowMapSampler,
    float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
    float blendArea = blendDistances.x;

    float4 comparison = pixelDepth.xxxx > cascadeDistances;

    int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
    currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

    int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

    float visibility = SampleShadowMapSimple(viewProj[currentCascadeIndex], worldPosition, 
    shadowMap, viewports[currentCascadeIndex], shadowMapSampler, bias[currentCascadeIndex]);

    float blendEnd = cascadeDistances[currentCascadeIndex];
    float blendStart = blendEnd - blendDistances[currentCascadeIndex];
    float alpha = smoothstep(blendStart, blendEnd, pixelDepth);

    float nextCascadeVisibility = (currentCascadeIndex == nextCascadeIndex || alpha == 0.f)
        ? 1.f
        : SampleShadowMapSimple(viewProj[nextCascadeIndex], worldPosition, shadowMap,
        viewports[nextCascadeIndex], shadowMapSampler, bias[nextCascadeIndex]);
    
    visibility = lerp(visibility, nextCascadeIndex, alpha);
    return visibility;
}

static float SampleCascadedShadowMapPCF(float4x4 viewProj[4], float3 worldPos, 
    Texture2D<float> shadowMap, float4 viewports[4], SamplerComparisonState shadowMapSampler,
    float2 texelSize, float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
    float4 comparison = pixelDepth.xxxx > cascadeDistances;

	int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

	int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

	float visibility = SampleShadowMapPCF(viewProj[currentCascadeIndex], worldPos, 
		shadowMap, viewports[currentCascadeIndex],
		shadowMapSampler, texelSize, bias[currentCascadeIndex]);

	float blendEnd = cascadeDistances[currentCascadeIndex];
	float blendStart = blendEnd - blendDistances[currentCascadeIndex];
	float alpha = smoothstep(blendStart, blendEnd, pixelDepth);

	float nextCascadeVisibility = (currentCascadeIndex == nextCascadeIndex || alpha == 0.f) 
		? 1.f // No need to sample next cascade, if we are the last cascade or if we are not in the blend area.
		: SampleShadowMapPCF(viewProj[nextCascadeIndex], worldPos, 
			shadowMap, viewports[nextCascadeIndex], 
			shadowMapSampler, texelSize, bias[nextCascadeIndex]);

	visibility = lerp(visibility, nextCascadeVisibility, alpha);
	return visibility;
}
#endif

#endif