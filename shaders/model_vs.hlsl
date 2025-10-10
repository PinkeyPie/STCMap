#include "common/model_rs.hlsli"

ConstantBuffer<TransformCb> transform : register(b0);

struct VsInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORDS;
    float3 normal   : NORMAL;
};

struct VsOutput
{
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float4 position : SV_Position;
};

[RootSignature(MODEL_RS)]
VsOutput main(VsInput vin)
{
    VsOutput vout;
    vout.position = mul(transform.Mvp, float4(vin.position, 1.f));
    vout.normal = mul(transform.M, float4(vin.normal, 0.f)).xyz;
    vout.uv = vin.uv;
    return vout;
}
