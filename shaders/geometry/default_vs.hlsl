#include "../rs/default_pbr_rs.hlsli"

ConstantBuffer<TransformCb> Transform : register(b0);

struct VsInput
{
    float3 Position : POSITION;
    float2 UV       : TEXCOORDS;
    float3 Normal   : NORMAL;
    float3 Tangent  : TANGENT;
};

struct VsOutput
{
    float2 UV       : TEXCOORDS;
    float3x3 TBN    : TANGENT_FRAME;
    float3 WorldPos : POSITION;
    float4 Pos      : SV_Position;
};

VsOutput main(VsInput vin) 
{
    VsOutput vout;
    vout.Pos = mul(Transform.MeshViewProj, float4(vin.Position, 1.f));

    vout.UV = vin.UV;
    vout.WorldPos = (mul(Transform.Mesh, float4(vin.Position, 1.f))).xyz;

    float3 normal = normalize(mul(Transform.Mesh, float4(vin.Normal, 0.f)).xyz);
    float3 tangent = normalize(mul(Transform.Mesh, float4(vin.Tangent, 0.f)).xyz);
    float3 bitangent = normalize(cross(normal, tangent));
    vout.TBN = float3x3(tangent, bitangent, normal);

    return vout;
}