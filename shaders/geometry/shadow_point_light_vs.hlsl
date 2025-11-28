#include "../rs/depth_only_rs.hlsli"


ConstantBuffer<PointShadowTransformCb> Transform : register(b0);

struct VsInput
{
	float3 Position		: POSITION;
	float2 UV			: TEXCOORDS;
	float3 Normal		: NORMAL;
	float3 Tangent		: TANGENT;
};

struct VsOutput
{
	float  ClipDepth	: CLIP_DEPTH;
	float4 Position		: SV_POSITION;
};

[RootSignature(POINT_SHADOW_RS)]
VsOutput main(VsInput vin)
{
	float3 position = mul(Transform.Mesh, float4(vin.Position, 1.f)).xyz;
	
	float3 L = position - Transform.LightPosition;

	L.z *= Transform.Flip;

	float l = length(L);
	L /= l;

	L.xy /= L.z + 1.f;

	VsOutput vout;
	vout.ClipDepth = L.z;
	vout.Position = float4(L.xy, l / Transform.MaxDistance, 1.f);
	return vout;
}