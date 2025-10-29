#include "rs/model_rs.hlsli"

ConstantBuffer<TransformCb> transform : register(b0);

struct VsInput
{
    float3 position : POSITION;
    float2 uv       : TEXCOORDS;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct VsOutput
{
    float2 uv               : TEXCOORDS;
    float3x3 tbn            : TANGENT_FRAME; // Tangent, bi-tangent, normal
    float3 worldPosition    : POSITION;
    float4 position         : SV_Position;
};

[RootSignature(MODEL_DEPTH_ONLY_RS)]
VsOutput main(VsInput vin)
{
    VsOutput vout;
    vout.position = mul(transform.ModelViewProj, float4(vin.position, 1.f));

    vout.uv = vin.uv;
    vout.worldPosition = (mul(transform.Model, float4(vin.position, 1.f))).xyz;

    float3 normal = normalize(mul(transform.Model, float4(vin.normal, 0.f)).xyz);
    float3 tangent = normalize(mul(transform.Model, float4(vin.tangent, 0.f)).xyz);
    float3 bitangent = normalize(cross(normal, tangent));
    vout.tbn = float3x3(tangent, bitangent, normal);

    return vout;
}
