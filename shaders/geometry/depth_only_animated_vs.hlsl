#include "../rs/depth_only_rs.hlsli"

ConstantBuffer<DepthOnlyTransformCb> Transform : register(b0);

struct MeshVertex
{
	float3 Position;
	float2 UV;
	float3 Normal;
	float3 Tangent;
};

StructuredBuffer<MeshVertex> PrevFrameVertices		: register(t0);

struct VsInput
{
	float3 Position		: POSITION;
	float2 UV			: TEXCOORDS;
	float3 Normal		: NORMAL;
	float3 Tangent		: TANGENT;

	uint VertexID       : SV_VertexID;
};

struct VsOutput
{
	float3 NDC				: NDC;
	float3 PrevFrameNDC		: PREV_FRAME_NDC;

	float4 Position			: SV_POSITION;
};

[RootSignature(ANIMATED_DEPTH_ONLY_RS)]
VsOutput main(VsInput vin)
{
	VsOutput vout;
	vout.Position = mul(Transform.MeshViewProj, float4(vin.Position, 1.f));
	vout.NDC = vout.Position.xyw;

	float3 prevFramePosition = PrevFrameVertices[vin.VertexID].Position;
	vout.PrevFrameNDC = mul(Transform.PrevFrameMVP, float4(prevFramePosition, 1.f)).xyw;
	return vout;
}