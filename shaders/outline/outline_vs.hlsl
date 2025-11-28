#include "../rs/outline_rs.hlsli"


ConstantBuffer<OutlineMarkerCb> Outline : register(b0);

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

[RootSignature(OUTLINE_MARKER_RS)]
VsOutput main(VsInput vin)
{
	VsOutput vout;
	vout.Position = mul(Outline.MVP, float4(vin.Position, 1.f));
	return vout;
}