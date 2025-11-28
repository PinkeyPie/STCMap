#include "../rs/depth_only_rs.hlsli"


ConstantBuffer<ShadowTransformCb> Transform : register(b0);

struct VsInput
{
	float3 Position		: POSITION;
	float2 UV			: TEXCOORDS;
	float3 Normal		: NORMAL;
	float3 Tangent		: TANGENT;
};

struct VsOutput
{
	float4 Position			: SV_POSITION;
};

[RootSignature(SHADOW_RS)]
VsOutput main(VsInput vin)
{
	VsOutput vout;
	vout.Position = mul(Transform.MeshViewProj, float4(vin.Position, 1.f));
	return vout;
}