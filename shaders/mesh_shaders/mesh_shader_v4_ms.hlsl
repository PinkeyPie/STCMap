#include "mesh_shader_v4_common.hlsli"
#include "../common/camera.hlsli"

#define MESH_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_VERTEX_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"CBV(b0), " \
"CBV(b1), " \
"SRV(t0), " \
"StaticSampler(s0, space=1), " \
"DescriptorTable(SRV(t0, space=1, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"


struct MeshOutput
{
	float4 Pos : SV_POSITION;
	float3 WorldPos : POSITION;
	float3 Normal : NORMAL;
};


ConstantBuffer<CameraCb> camera								: register(b1);
StructuredBuffer<MarchingCubesLookup> marchingCubesLookup     : register(t0);


struct CubeCorner
{
	float3 Normal;
	float Value;
};

groupshared CubeCorner Corners[8];

struct SingleUnpack
{
    uint Index;
    uint ShiftDown;
};

struct UnpackInfo
{
    SingleUnpack SingleUnpacks[3];
};

static const UnpackInfo unpack[5] =
{
    { { 0, 0 }, { 0, 8 }, { 0, 16 } },
    { { 0, 24 }, { 1, 0 }, { 1, 8 } },
    { { 1, 16 }, { 1, 24 }, { 2, 0 } },
    { { 2, 8 }, { 2, 16 }, { 2, 24 } },
    { { 3, 0 }, { 3, 8 }, { 3, 16 } },
};


[RootSignature(MESH_RS)]
[outputtopology("triangle")]
[numthreads(32, 1, 1)]
void main(
	in uint groupID : SV_GroupID,
	in uint groupThreadID : SV_GroupThreadID,
	in payload MeshPayload p,
	out vertices MeshOutput outVerts[12],
	out indices uint3 outTriangles[5]
)
{
	const uint meshletID = p.MeshletIDs[groupID];

	// Convert linear meshletID into x, y and z coordinates
	uint miX = (meshletID >> (0 * SHIFT)) & (GRID_SIZE - 1);
	uint miY = (meshletID >> (1 * SHIFT)) & (GRID_SIZE - 1);
	uint miZ = (meshletID >> (2 * SHIFT));

	// Corner positions of the cube
	float3 cornerPos0 = float3(miX, miY, miZ) * STEP_SIZE;
	float3 cornerPos1 = cornerPos0 + STEP_SIZE;

	// First 8 lanes evaluate the field for each corner, the other 24 evaluate at offsetted positions in x, y and z so we can compute a normal at each corner from the field gradient
	float3 pos = float3((groupThreadID & 1) != 0 ? cornerPos1.x : cornerPos0.x, (groupThreadID & 2) != 0 ? cornerPos1.y : cornerPos0.y, (groupThreadID & 4) != 0 ? cornerPos1.z : cornerPos0.z);

	const float epsilon = (1.f / 16.f) * STEP_SIZE;
	uint off = groupThreadID / 8;
	pos.x += (off == 1) ? epsilon : 0.f;
	pos.y += (off == 2) ? epsilon : 0.f;
	pos.z += (off == 3) ? epsilon : 0.f;

	// Evaluate the field function for all 32 lanes
	float value = field(pos);

	// Grab data from other lanes so we can compute the normal by computing the difference in value in x, y and z directions.
	float3 normal;
	normal.x = WaveReadLaneAt(value, WaveGetLaneIndex() +  8);
	normal.y = WaveReadLaneAt(value, WaveGetLaneIndex() + 16);
	normal.z = WaveReadLaneAt(value, WaveGetLaneIndex() + 24);

	// Save intermediates to LDS. Corner positions can be computed directly from indices, so no need to put in LDS.
	if (groupThreadID < 8)
	{
		CubeCorner corner;
		corner.Normal = normalize(value - normal);
		corner.Value = value;
		Corners[groupThreadID] = corner;
	}

	// Look up the Marching Cubes index from the signs of corner nodes. Only the lowest 8 lanes are relevant here, hence the 0xFF mask.
	uint4 ballot = WaveActiveBallot(value >= 0.f);
	uint index = (ballot.x & 0xFF);

	const uint vertexCount = uint(marchingCubesLookup[index].TriangleAndVertexCount >> 16);
	const uint triangleCount = marchingCubesLookup[index].TriangleAndVertexCount & 0xFFFF;

	SetMeshOutputCounts(vertexCount, triangleCount);

	// Output up to 12 vertices, one lane per vertex.
	if (groupThreadID < vertexCount)
	{
		// Look up the corner indices for this edge
		uint lookupWord = groupThreadID >> 2; // Divide by 4.
		uint lookupID = groupThreadID & 0x3;  // Modulo 4.

		uint word = marchingCubesLookup[index].Vertices[lookupWord];
		uint edge = (word >> (lookupID * 8)) & 0xFF;

		uint i0 = edge & 0x7;
		uint i1 = edge >> 3;

		// Corner positions
		float3 pos0 = float3((i0 & 1) != 0 ? cornerPos1.x : cornerPos0.x, (i0 & 2) != 0 ? cornerPos1.y : cornerPos0.y, (i0 & 4) != 0 ? cornerPos1.z : cornerPos0.z);
		float3 pos1 = float3((i1 & 1) != 0 ? cornerPos1.x : cornerPos0.x, (i1 & 2) != 0 ? cornerPos1.y : cornerPos0.y, (i1 & 4) != 0 ? cornerPos1.z : cornerPos0.z);

		// Interpolate position and normal
		float t = Corners[i0].Value / (Corners[i0].Value - Corners[i1].Value);
		float3 pos = lerp(pos0, pos1, t);
		float3 normal = lerp(Corners[i0].Normal, Corners[i1].Normal, t);

		// Output final vertex
		pos *= 20.f;
		outVerts[groupThreadID].Pos = mul(camera.ViewProj, float4(pos, 1.f));
		outVerts[groupThreadID].WorldPos = pos;
		outVerts[groupThreadID].Normal = normal;
	}

	// Output up to 5 triangles, one lane per triangle. 
	if (groupThreadID < triangleCount)
	{
		UnpackInfo info = unpack[groupThreadID];

		uint packedIndex = marchingCubesLookup[index].Indices[groupThreadID];

		outTriangles[groupThreadID] = uint3
		(
			(marchingCubesLookup[index].Indices[info.SingleUnpacks[0].Index] >> (info.SingleUnpacks[0].ShiftDown)) & 0xFF,
			(marchingCubesLookup[index].Indices[info.SingleUnpacks[1].Index] >> (info.SingleUnpacks[1].ShiftDown)) & 0xFF,
			(marchingCubesLookup[index].Indices[info.SingleUnpacks[2].Index] >> (info.SingleUnpacks[2].ShiftDown)) & 0xFF
		);
	}
}
