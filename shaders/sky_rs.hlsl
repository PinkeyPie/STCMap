#include "rs/sky_rs.hlsli"

ConstantBuffer<SkyCb> Sky : register(b0);

struct VsInput
{
    float3 position : POSITION;
};

struct VsOutput
{
    float3 uv       : TEXCOORDS;
    float4 position : SV_Position;
};

VsOutput main(VsInput vin)
{
    VsOutput vout;

    vout.uv = vin.position;
    vout.position = mul(Sky.ViewProj, float4(vin.position, 1.f));
    vout.position.z = vout.position.w - 1e-6f;

    return vout;
}