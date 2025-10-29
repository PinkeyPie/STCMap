#include "rs/model_rs.hlsli"
#include "common/brdf.hlsli"

struct PsInput
{
    float2 uv            : TEXCOORDS;
    float3x3 tbn         : TANGENT_FRAME;
    float3 worldPosition : POSITION;
    float4 position      : SV_Position;
};

ConstantBuffer<PbrMaterialCb> material : register(b1);

SamplerState WrapSampler                : register(s0);
SamplerState ClampSampler               : register(s1);

Texture2D<float4> AlbedoTex             : register(t0);
Texture2D<float3> NormalTex             : register(t1);
Texture2D<float> RoughTex               : register(t2);
Texture2D<float> MetalTex               : register(t3);

TextureCube<float4> IrradianceTexture   : register(t0, space1);
TextureCube<float4> EnvironmentTexture  : register(t1, space1);

Texture2D<float4> Brdf                  : register(t0, space2);

// Todo
static const float3 L = normalize(float3(1.f, 0.f, 0.3f));
static const float3 SunColor = float3(1.f, 1.f, 1.f) * 50.f;
static const float3 CameraPosition = float3(0.f, 0.f, 4.f);

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

    float3 camToPos = pin.worldPosition - CameraPosition.xyz;
    float3 V = -normalize(camToPos);
    float R0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.xyz, metallic);

    float4 totalLighting = float4(0.f, 0.f, 0.f, albedo.w);

    totalLighting.xyz += CalculateAmbientLighting(albedo.xyz, IrradianceTexture, EnvironmentTexture, Brdf, ClampSampler, N, V, R0, roughness, metallic, ao);

    float visibility = 1.f;
    float3 radiance = SunColor.xyz * visibility; // No attenuation for sun.

    totalLighting.xyz += CalculateDirectLighting(albedo.xyz, radiance, N, L, V, R0, roughness, metallic);

    return totalLighting;
}