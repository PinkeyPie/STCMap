#ifndef LIGHTING_H
#define LIGHTING_H

struct LightAttenuation 
{
    float c;
    float l;
    float q;
};

struct DirectionalLighting
{
    float4x4 ViewProj[4];
    float4 CascadeDistances;
    float4 Bias;

    float4 WorldSpaceDirection;
    float4 Color;

    uint NumShadowCascades;
    float BlendArea;
    float TexelSize;
    uint ShadowMapDimesions;
};

struct SpotLight
{
    float4x4 ViewProj;

    float4 WorldSpacePosition;
    float4 WorldSpaceDirection;
    float4 Color;

    LightAttenuation Attenuation;

    float InnerAngle;
    float OuterAngle;
    float InnerCutoff;
    float OuterCutoff;
    float TexelSize;
    float Bias;
};

struct 
{
    float4 WorldSpacePositionAndRadius;
    float4 Color;
};

static float GetAttenuation(LightAttenuation a, float distance) 
{
    return 1.f / (a.c + a.l * distance + a.q * distance * distance);
}

static float SampleShadowMap(float4x4 viewProj, float3 worldPosition, Texture2D<float> shadowMap, SamplerComparisonState shadowMapSampler,
    float texelSize, float bias)
{
    float4 lightProjected = mul(viewProj, float4(worldPosition, 1.f));
    lightProjected.xyz /= lightProjected.w;

    float2 lightUV = lightProjected.xy + 0.5f + float2(0.5f, 0.5f);
    lightUV.y = 1.f - lightUV.y;

    float visibility = 0.f;

    for(float y = -1.5f; y <= 1.5f; y += 1.f)
    {
        for(float x = -1.5f; x < 1.5f; x += 1.f) 
        {
            visibility += shadowMap.SampleCmpLevelZero(shadowMapSampler, lightUV + float2(x, y) * texelSize, lightProjected.z - bias);
        }
    }
    visibility /= 16.f;
    return visibility;
}

#endif