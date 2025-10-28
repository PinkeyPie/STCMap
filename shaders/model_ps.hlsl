#include "rs/model_rs.hlsli"

struct PsInput
{
    float2 uv     : TEXCOORD;
    float3 normal : NORMAL;
};

SamplerState TexSampler : register(s0);
Texture2D<float4> Tex   : register(t0);

[RootSignature(MODEL_RS)]
float4 main(PsInput pin) : SV_Target
{
    // float4 albedo = Tex.Sample(TexSampler, pin.uv) * 10.f;
    float4 albedo = float4(1.f, 0.f, 0.f, 0.f) * 10.f;

    static const float3 L = normalize(float3(1.f, 0.8f, 0.3f));
    return clamp(dot(L, normalize(pin.normal)), 0.1f, 1.f) * albedo;
}