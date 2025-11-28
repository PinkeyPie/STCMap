#include "../common/transform.hlsli"

#define MESH_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_VERTEX_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS | DENY_PIXEL_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=32, b0), " \
"SRV(t0), " \
"SRV(t1), " \
"SRV(t2), " \
"SRV(t3)"

struct MeshOutput
{
	float4 Pos : SV_POSITION;
	float3 Color : COLOR0;
};

struct MeshVertex
{
    float3 Position;
    float3 Normal;
};

struct MeshletInfo
{
    uint NumVertices;
    uint FirstVertex;
    uint NumPrimitives;
    uint FirstPrimitive;
};

ConstantBuffer<TransformCb> Transform      : register(b0);

StructuredBuffer<MeshVertex> Vertices      : register(t0);
StructuredBuffer<MeshletInfo> Meshlets     : register(t1);
StructuredBuffer<uint> UniqueVertexIndices  : register(t2);
StructuredBuffer<uint> PrimitiveIndices     : register(t3);

[outputtopology("triangle")]
[numthreads(128, 1, 1)]
[RootSignature(MESH_RS)]
void main(
    in uint groupThreadID : SV_GroupThreadID, 
    in uint groupID : SV_GroupID, 
    out vertices MeshOutput outVerts[64], 
    out indices uint3 outIndices[126]
)
{
    MeshletInfo m = Meshlets[groupID];

    SetMeshOutputCounts(m.NumVertices, m.NumPrimitives);

    if (groupThreadID < m.NumPrimitives)
    {
        uint packedPrimitive = PrimitiveIndices[m.FirstPrimitive + groupThreadID];

        // Unpacks a 10 bits per index triangle from a 32-bit uint.
        outIndices[groupThreadID] = uint3(packedPrimitive & 0x3FF, (packedPrimitive >> 10) & 0x3FF, (packedPrimitive >> 20) & 0x3FF);
    }

    if (groupThreadID < m.NumVertices)
    {
        uint vertexIndex = UniqueVertexIndices[m.FirstVertex + groupThreadID];
        uint meshletIndex = groupID;

        // Color based on meshlet index for visualization.
        outVerts[groupThreadID].Color = float3(
            float(meshletIndex & 1),
            float(meshletIndex & 3) / 4,
            float(meshletIndex & 7) / 8);

        outVerts[groupThreadID].Pos = mul(Transform.MeshViewProj, float4(Vertices[vertexIndex].Position, 1.f));
    }
}