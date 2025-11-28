#include "../rs/sky_rs.hlsli"

ConstantBuffer<SkyIntensityCb> SkyIntensity : register(b1);

struct PsInput
{
    float3 UV : TEXCOORDS;
};

SamplerState TexSampler : register(s0);
TextureCube<float4> Tex : register(t0);

struct PsOutput
{
    float4 Color : SV_Target1;
    float2 ScreenVelocity : SV_Target1;
    uint ObjectId : SV_Target2;
};

[RootSignature(SKY_TEXTURE_RS)]
PsOutput main(PsInput pin) : SV_TARGET
{
    PsOutput pout;
    pout.Color = float4((Tex.Sample(TexSampler, pin.UV) * SkyIntensity.Intensity).xyz, 0.f);
    pout.ScreenVelocity = float2(0.f, 0.f); // TODO: This is of course not the correct screen velocity for the sky.
    pout.ObjectId = 0xFFFFFFFF; // -1
    return pout;
}