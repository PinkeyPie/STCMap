#include "common/present_rs.hlsli"

ConstantBuffer<TonemapCb> tonemap : register(b0);

struct PsInput
{
    float2 uv : TEXCOORDS;
};

[RootSignature(PRESENT_RS)]
float4 main(PsInput pin) : SV_Target
{
    return float4(pin.uv * tonemap.Exposure, 0.f, 1.f);
}