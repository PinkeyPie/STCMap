#include "common/model_rs.hlsli"

struct PsInput
{
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

[RootSignature(MODEL_RS)]
float4 main(PsInput pin) : SV_Target
{
    static const float3 L = normalize(float3(1.f, 0.8f, 0.3f));
    return saturate(dot(L, normalize(pin.normal))) * float4(1.f, 1.f, 1.f, 1.f);
}