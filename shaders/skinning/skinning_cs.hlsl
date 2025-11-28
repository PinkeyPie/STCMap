#include "../common/cs.hlsli"
#include "../rs/skinning_rs.hlsli"


#define BLOCK_SIZE 512

struct MeshVertex
{
	float3 Position;
	float2 UV;
	float3 Normal;
	float3 Tangent;
};

struct SkinnedMeshVertex
{
	float3 Position;
	float2 UV;
	float3 Normal;
	float3 Tangent;
	uint SkinIndices;
	uint SkinWeights;
};

ConstantBuffer<SkinningCb> SkinningCB				: register(b0);

StructuredBuffer<SkinnedMeshVertex> InputVertices	: register(t0);

StructuredBuffer<float4x4> SkinningMatrices			: register(t1);
RWStructuredBuffer<MeshVertex> OutputVertices		: register(u0);


[RootSignature(SKINNING_RS)]
[numthreads(BLOCK_SIZE, 1, 1)]
void main(CsInput cin)
{
	uint index = cin.DispatchThreadId.x;

	if (index >= SkinningCB.NumVertices)
	{
		return;
	}

	SkinnedMeshVertex vertex = InputVertices[SkinningCB.FirstVertex + index];

	uint4 skinIndices = uint4(
		vertex.SkinIndices >> 24,
		(vertex.SkinIndices >> 16) & 0xFF,
		(vertex.SkinIndices >> 8) & 0xFF,
		vertex.SkinIndices & 0xFF
		);

	float4 skinWeights = float4(
		vertex.SkinWeights >> 24,
		(vertex.SkinWeights >> 16) & 0xFF,
		(vertex.SkinWeights >> 8) & 0xFF,
		vertex.SkinWeights & 0xFF
		) * (1.f / 255.f);

	skinWeights /= dot(skinWeights, (float4)1.f);

	skinIndices += SkinningCB.FirstJoint.xxxx;

	float4x4 s =
		SkinningMatrices[skinIndices.x] * skinWeights.x +
		SkinningMatrices[skinIndices.y] * skinWeights.y +
		SkinningMatrices[skinIndices.z] * skinWeights.z +
		SkinningMatrices[skinIndices.w] * skinWeights.w;

	float3 position = mul(s, float4(vertex.Position, 1.f)).xyz;
	float3 normal = mul(s, float4(vertex.Normal, 0.f)).xyz;
	float3 tangent = mul(s, float4(vertex.Tangent, 0.f)).xyz;


	MeshVertex output = { position, vertex.UV, normal, tangent };
	OutputVertices[SkinningCB.WriteOffset + index] = output;
}
