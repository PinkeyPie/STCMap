#include "../common/common.hlsli"
#include "../rs/post_processing_rs.hlsli"

struct PsInput
{
    float2 UV : TEXCOORDS;
};

SamplerState texSampler : register(s0);
Texture2D<float4> Input : register(t0);

[RootSignature(PRESENT_RS)]
float4 main(PsInput pin) : SV_Target
{
    return float4(Input.Sample(texSampler, pin.UV).rgb, 1.f);
}