#include "rs/outline_rs.hlsli"

ConstantBuffer<OutlineCb> Outline : register(b0);

struct VsInput
{
    float3 position : POSITION;
    float3 uv       : TEXCOORDS;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct VsOutput
{
    float4 color    : COLOR;
    float4 position : SV_Position;
};

[RootSignature(OUTLINE_RS)]
VsOutput main(VsInput vin)
{
    VsOutput vout;
    vout.position = mul(Outline.MeshViewProj, float4(vin.position + vin.normal * 1.2f, 1.f));
    vout.color = Outline.Color;
    return vout;
}