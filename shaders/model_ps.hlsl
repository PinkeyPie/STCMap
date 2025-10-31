#include "rs/model_rs.hlsli"
#include "common/brdf.hlsli"
#include "common/camera.hlsli"
#include "rs/light_culling.hlsli"

struct PsInput
{
    float2 uv             : TEXCOORDS;
    float3x3 tbn          : TANGENT_FRAME;
    float3 worldPosition  : POSITION;
    float4 screenPosition : SV_Position;
};

ConstantBuffer<PbrMaterialCb> material : register(b1);
ConstantBuffer<CameraCb> camera        : register(b2);

SamplerState WrapSampler                : register(s0);
SamplerState ClampSampler               : register(s1);

Texture2D<float4> AlbedoTex             : register(t0);
Texture2D<float3> NormalTex             : register(t1);
Texture2D<float> RoughTex               : register(t2);
Texture2D<float> MetalTex               : register(t3);

TextureCube<float4> IrradianceTexture   : register(t0, space1);
TextureCube<float4> EnvironmentTexture  : register(t1, space1);

Texture2D<float4> Brdf                  : register(t0, space2);

Texture2D<uint2> LightGrid                              : register(t0, space3);
StructuredBuffer<uint> LightIndexList                   : register(t1, space3);
StructuredBuffer<PointLightBoundingVolume> PointLights  : register(t2, space3);
StructuredBuffer<SpotLightBoundingVolume> SpotLights    : register(t3, space3);

// Todo
static const float3 L = normalize(float3(1.f, 0.f, 0.3f));
static const float3 SunColor = float3(1.f, 1.f, 1.f) * 50.f;

[RootSignature(MODEL_RS)]
float4 main(PsInput pin) : SV_Target
{
    uint flags = material.Flags;

    float4 albedo = ((flags & USE_ALBEDO_TEXTURE) ? 
        AlbedoTex.Sample(WrapSampler, pin.uv) : float4(1.f, 1.f, 1.f, 1.f)) 
        * material.AlbedoTint;

    float3 N = (flags & USE_NORMAL_TEXTURE) 
        ? mul(NormalTex.Sample(WrapSampler, pin.uv).xyz * 2.f - float3(1.f, 1.f, 1.f), pin.tbn)
        : pin.tbn[2];
    
    float roughness = (flags & USE_ROUGHNESS_TEXTURE)
        ? RoughTex.Sample(WrapSampler, pin.uv)
        : material.RoughnessOverride;
    roughness = clamp(roughness, 0.01f, 0.99f);

    float metallic = (flags & USE_METALLIC_TEXTURE)
        ? MetalTex.Sample(WrapSampler, pin.uv)
        : material.MetallicOverride;

    float ao = 1.f;

    const uint2 tileIndex = uint2(floor(pin.screenPosition.xy / LIGHT_CULLING_TILE_SIZE));
    const uint2 lightIndexData = LightGrid.Load(int3(tileIndex, 0));
    const uint lightOffset = lightIndexData.x;
    const uint lightCount = lightIndexData.y;

    float3 cameraPosition = camera.Position.xyz;
    float3 camToPos = pin.worldPosition - cameraPosition;
    float3 V = -normalize(camToPos);
    float R0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.xyz, metallic);

    float4 totalLighting = float4(0.f, 0.f, 0.f, albedo.w);

    for(uint lightIndex = lightOffset; lightIndex < lightOffset + lightCount; lightIndex++) 
    {
        uint index = LightIndexList[lightIndex];
        PointLightBoundingVolume pl = PointLights[index];
        if(length(pl.Position - pin.worldPosition) < pl.Radius)
        {
            float3 radiance = float3(50.f, 0.f, 0.f);
            totalLighting.xyz += CalculateDirectLighting(albedo.xyz, radiance, N, L, V, R0, roughness, metallic);
        }
    }

    totalLighting.xyz += CalculateAmbientLighting(albedo.xyz, IrradianceTexture, EnvironmentTexture, Brdf, ClampSampler, N, V, R0, roughness, metallic, ao);

    float visibility = 1.f;
    float3 radiance = SunColor.xyz * visibility; // No attenuation for sun.

    totalLighting.xyz += CalculateDirectLighting(albedo.xyz, radiance, N, L, V, R0, roughness, metallic);

    return totalLighting;
}