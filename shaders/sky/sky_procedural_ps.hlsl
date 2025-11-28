#include "../common/math.hlsli"
#include "../rs/sky_rs.hlsli"

ConstantBuffer<SkyIntensityCb> SkyIntensity : register(b1);

struct PsInput
{
    float3 UV : TEXCOORDS;
};

struct PsOutput
{
    float4 Color : SV_Target0;
    float2 ScreenVelocity : SV_Target1;
    uint ObjectId : SV_Target2;
};

[RootSignature(SKY_PROCEDURAL_RS)]
PsOutput main(PsInput pin) : SV_TARGET
{
    float3 dir = normalize(pin.UV);
    float2 panoUV = float2(atan2(-dir.x, -dir.z), acos(dir.y)) * INV_ATAN;

    float step = 1.f / 20.f;

    int x = (int)(panoUV.x / step) & 1;
    int y = (int)(panoUV.y / step) & 1;

    float intensity = remap((float)(x == y), 0.f, 1.f, 0.05f, 1.f) * SkyIntensity.Intensity;

    PsOutput pout;
    pout.Color = float4(intensity * float3(0.4f, 0.6f, 0.2f), 0.f);
    pout.ScreenVelocity = float2(0.f, 0.f); // TODO: This is of course not the correct screen velocity for the sky.
    pout.ObjectId = 0xffffffff; // -1.

    return pout;
}