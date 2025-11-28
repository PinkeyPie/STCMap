#include "../rs/depth_only_rs.hlsli"


ConstantBuffer<DepthOnlyTransformCb> Transform : register(b0);

struct VsInput
{
	float3 Position		: POSITION;
	float2 UV			: TEXCOORDS;
	float3 Normal		: NORMAL;
	float3 Tangent		: TANGENT;
};

struct VsOutput
{
	float3 NDC				: NDC;
	float3 PrevFrameNDC		: PREV_FRAME_NDC;

	float4 Position			: SV_POSITION;
};

[RootSignature(DEPTH_ONLY_RS)]
VsOutput main(VsInput vin)
{
	VsOutput vout;
	vout.Position = mul(Transform.MeshViewProj, float4(vin.Position, 1.f));
	vout.NDC = vout.Position.xyw;
	vout.PrevFrameNDC = mul(Transform.PrevFrameMVP, float4(vin.Position, 1.f)).xyw;
	return vout;
}