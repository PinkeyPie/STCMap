#include "../rs/flat_simple_rs.hlsli"
#include "../common/transform.hlsli"

ConstantBuffer<TransformCb> Transform : register(b0);

struct VsInput
{
	float3 Position			: POSITION;
	float3 Normal			: NORMAL;
};

struct VsOutput
{
	float3 WorldPosition	: WORLD_POSITION;
	float3 WorldNormal		: WORLD_NORMAL;
	float4 Position			: SV_POSITION;
};

VsOutput main(VsInput vin)
{
	VsOutput vout;
	vout.Position = mul(Transform.MeshViewProj, float4(vin.Position, 1.f));
	vout.WorldPosition = mul(Transform.Mesh, float4(vin.Position, 1.f)).xyz;
	vout.WorldNormal = mul(Transform.Mesh, float4(vin.Normal, 0.f)).xyz;
	return vout;
}