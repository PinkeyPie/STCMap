#include "rs/sky_rs.hlsli"

struct PsInput
{
    float3 uv : TEXCOORDS;
};

SamplerState TexSampler : register(s0);
TextureCube<float4> Tex : register(t0);

[RootSignature(SKY_TEXTURE_RS)]
float4 main(PsInput pin) : SV_TARGET
{
    return Tex.Sample(TexSampler, pin.uv);
}