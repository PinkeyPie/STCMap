#include "geometry.h"

#include <valarray>

struct VertexInfo {
	uint32 VertexSize;

	uint32 PositionOffset;
	uint32 UvOffset;
	uint32 NormalOffset;
	uint32 TangentOffset;
	uint32 SkinOffset;
};

namespace {
	VertexInfo GetVertexInfo(uint32 flags) {
		VertexInfo result = {};
		if (flags & EMeshCreationFlagsWithPositions) {
			result.PositionOffset = result.VertexSize;
			result.VertexSize += sizeof(vec3);
		}
		if (flags & EMeshCreationFlagsWithUvs) {
			result.UvOffset = result.VertexSize;
			result.VertexSize += sizeof(vec2);
		}
		if (flags & EMeshCreationFlagsWithNormals) {
			result.NormalOffset = result.VertexSize;
			result.VertexSize += sizeof(vec3);
		}
		if (flags & EMeshCreationFlagsWithTangents) {
			result.TangentOffset = result.VertexSize;
			result.VertexSize += sizeof(vec3);
		}
		if (flags & EMeshCreationFlagsWithSkin) {
			result.SkinOffset = result.VertexSize;
			result.VertexSize += sizeof(SkinningWeights);
		}

		return result;
	}
}

CpuMesh::CpuMesh(uint32 flags) {
	Flags = flags;
	VertexInfo info = GetVertexInfo(flags);
	VertexSize = info.VertexSize;
	SkinOffset = info.SkinOffset;
}

CpuMesh::CpuMesh(CpuMesh&& mesh) {
	Flags = mesh.Flags;
	VertexSize = mesh.VertexSize;
	SkinOffset = mesh.SkinOffset;
	Vertices = mesh.Vertices;
	Triangles = mesh.Triangles;
	NumVertices = mesh.NumVertices;
	NumTriangles = mesh.NumTriangles;

	mesh.Vertices = 0;
	mesh.Triangles = 0;
}

CpuMesh::~CpuMesh() {
	if (Vertices) {
		_aligned_free(Vertices);
	}
	if (Triangles) {
		_aligned_free(Triangles);
	}
}

void CpuMesh::AlignNextTriangle() {
	// This is called when a new mesh is pushed. The function aligns the next index to a 16-byte boundary.
	NumTriangles = AlignTo(NumTriangles, 8); // 8 triangles are 48 bytes, which is divisible by 16.
}

void CpuMesh::Reserve(uint32 vertexCount, uint32 triangleCount) {
	Vertices = (uint8*)_aligned_realloc(Vertices, (NumVertices + vertexCount) * VertexSize, 64);
	Triangles = (IndexedTriangle16*)_aligned_realloc(Triangles, (NumTriangles + triangleCount + 8) * sizeof(IndexedTriangle16), 64); // Allocate 8 more, such that we can align without problems.
}

/*
#define PushVertex(position, uv, normal, tangent, skin) \
	if(Flags & EMeshCreationFlagsWithPositions) { *(vec3*)vertexPtr = position; vertexPtr += sizeof(vec3); } \
	if(Flags & EMeshCreationFlagsWithUvs) { *(vec2*)vertexPtr = uv; vertexPtr += sizeof(vec3); } \
	if(Flags & EMeshCreationFlagsWithNormals) { *(vec3*)vertexPtr = normal; vertexPtr += sizeof(vec3); } \
	if(Flags & EMeshCreationFlagsWithTangents) { *(vec3*)vertexPtr = tangent; vertexPtr += sizeof(vec3); } \
	if(Flags & EMeshCreationFlagsWithSkin) { *(SkinningWeights*)vertexPtr = skin; vertexPtr += sizeof(vec3); } \
	++NumVertices
*/

void CpuMesh::PushVertex(vec3 position, vec2 uv, vec3 normal, vec3 tangent, SkinningWeights skin) {
	uint8* ptrVertex = Vertices + VertexSize * NumVertices;
	if (Flags & EMeshCreationFlagsWithPositions) {
		*reinterpret_cast<vec3*>(ptrVertex) = position;
		ptrVertex += sizeof(vec3);
	}
	if (Flags & EMeshCreationFlagsWithUvs) {
		*(vec2*)ptrVertex = uv;
		ptrVertex += sizeof(vec3);
	}
	if (Flags & EMeshCreationFlagsWithNormals) {
		*(vec3*)ptrVertex = normal;
		ptrVertex += sizeof(vec3);
	}
	if (Flags & EMeshCreationFlagsWithTangents) {
		*(vec3*)ptrVertex = tangent;
		ptrVertex += sizeof(vec3);
	}
	if (Flags & EMeshCreationFlagsWithSkin) {
		*(SkinningWeights*)ptrVertex = skin;
		ptrVertex += sizeof(vec3);
	}
	++NumVertices;
}


void CpuMesh::PushTriangle(uint16 a, uint16 b, uint16 c) {
	Triangles[NumTriangles++] = { a, b, c };
}

SubmeshInfo CpuMesh::PushQuad(vec2 radius) {
	AlignNextTriangle();

	uint32 baseVertex = NumVertices;
	uint32 firstTriangle = NumTriangles;

	Reserve(4, 2);

	uint8* vertexPtr = Vertices + VertexSize * NumVertices;

	PushVertex(vec3(-radius.x, -radius.y, 0.f), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	PushVertex(vec3(radius.x, -radius.y, 0.f), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	PushVertex(vec3(-radius.x, radius.y, 0.f), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	PushVertex(vec3(radius.x, radius.y, 0.f), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
	
	PushTriangle(0, 1, 2);
	PushTriangle(1, 3, 2);
	
	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 2;
	result.BaseVertex = baseVertex;
	result.NumVertices = 4;
	return result;
}

SubmeshInfo CpuMesh::PushCube(vec3 radius, bool flipWindingOrder) {
	AlignNextTriangle();

	uint32 baseVertex = NumVertices;
	uint32 firstTriangle = NumTriangles;

	if ((Flags & EMeshCreationFlagsWithPositions)
		and not(Flags & EMeshCreationFlagsWithUvs)
		and not(Flags & EMeshCreationFlagsWithNormals)
		and not(Flags & EMeshCreationFlagsWithTangents)) {
		Reserve(8, 12);

		uint8* vertexPtr = Vertices + VertexSize * NumVertices;

		PushVertex(vec3(-radius.x, -radius.y, radius.z), {}, {}, {}, {}); // 0
		PushVertex(vec3(radius.x, -radius.y, radius.z), {}, {}, {}, {}); // x
		PushVertex(vec3(-radius.x, radius.y, radius.z), {}, {}, {}, {}); // y
		PushVertex(vec3(radius.x, radius.y, radius.z), {}, {}, {}, {}); // xy
		PushVertex(vec3(-radius.x, -radius.y, radius.z), {}, {}, {}, {}); // xz
		PushVertex(vec3(radius.x, -radius.y, -radius.z), {}, {}, {}, {}); // xz
		PushVertex(vec3(-radius.x, radius.y, -radius.z), {}, {}, {}, {}); // yz
		PushVertex(vec3(radius.x, radius.y, -radius.z), {}, {}, {}, {}); // xuz

		PushTriangle(0, 1, 2);
		PushTriangle(1, 3, 2);
		PushTriangle(1, 5, 3);
		PushTriangle(5, 7, 3);
		PushTriangle(5, 4, 7);
		PushTriangle(4, 6, 7);
		PushTriangle(4, 0, 6);
		PushTriangle(0, 2, 6);
		PushTriangle(2, 3, 6);
		PushTriangle(3, 7, 6);
		PushTriangle(4, 5, 0);
		PushTriangle(5, 1, 0);
	}
	else {
		Reserve(24, 32);

		uint8* vertexPtr = Vertices + VertexSize * NumVertices;

		PushVertex(vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(vec3(-radius.x, radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(vec3(radius.x, radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(vec3(radius.x, -radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

		PushTriangle(0, 1, 2);
		PushTriangle(1, 3, 2);
		PushTriangle(4, 5, 6);
		PushTriangle(5, 7, 6);
		PushTriangle(8, 9, 10);
		PushTriangle(9, 11, 10);
		PushTriangle(12, 13, 14);
		PushTriangle(13, 15, 14);
		PushTriangle(16, 17, 18);
		PushTriangle(17, 19, 18);
		PushTriangle(20, 21, 22);
		PushTriangle(21, 23, 22);
	}

	if (flipWindingOrder) {
		for (uint32 i = NumTriangles - 12; i < NumTriangles; i++) {
			uint16 tmp = Triangles[i].B;
			Triangles[i].B = Triangles[i].C;
			Triangles[i].C = tmp;
		}
	}

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 12;
	result.BaseVertex = baseVertex;
	result.NumVertices = NumVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushSphere(uint16 slices, uint16 rows, float radius) {
	AlignNextTriangle();

	uint32 baseVertex = NumVertices;
	uint32 firstTriangle = NumTriangles;

	assert(slices > 2);
	assert(rows > 0);

	float vertDeltaAngle = M_PI / (rows + 1);
	float horzDeltaAngle = 2.f * M_PI / slices;

	assert(slices * rows + 2 <= UINT16_MAX);

	Reserve(slices * rows + 2, 2 * rows * slices);

	// Vertices
	PushVertex(vec3(0.f, -radius, 0.f), DirectionToPanoramaUv(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	for (uint32 y = 0; y < rows; y++) {
		float vertAngle = (y + 1) * vertDeltaAngle - M_PI;
		float vertexY = cosf(vertAngle);
		float currentCirceRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; x++) {
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCirceRadius;
			float vertexZ = sinf(horzAngle) * currentCirceRadius;
			vec3 pos(vertexX * radius, vertexY * radius, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);

			vec2 uv = DirectionToPanoramaUv(nor);
			PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}

	PushVertex(vec3(0.f, radius, 0.f), DirectionToPanoramaUv(vec3(0.f, 1.f, 0.f)), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	uint16 lastVertex = slices * rows + 2;
	
	// Indices.
	for (uint16 x = 0; x < slices - 1; x++) {
		PushTriangle(0, x + 1u, x + 2u);
	}
	PushTriangle(0, slices, 1);

	for (uint16 y = 0; y < rows - 1; y++) {
		for (uint16 x = 0; x < slices - 1; x++) {
			PushTriangle(y * slices + 1u + x, (y + 1u) * slices + 2u + x, y * slices + 2u + x);
			PushTriangle(y * slices + 1u + x, (y + 1u) * slices + 1u + x, (y + 1u) * slices + 2u + x);
		}
		PushTriangle((uint16)(y * slices + slices), (uint16)((y + 1u) * slices + 1u), (uint16)(y * slices + 1u));
		PushTriangle((uint16)(y * slices + slices), (uint16)((y + 1u) * slices + slices), (uint16)((y + 1u) * slices + 1u));
	}
	for (uint16 x = 0; x < slices - 1; x++) {
		PushTriangle(lastVertex - 2u - x, lastVertex - 3u - x, lastVertex - 1u);
	}
	PushTriangle(lastVertex - 1u - slices, lastVertex - 2u, lastVertex - 1u);

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 2 * rows * slices;
	result.BaseVertex = baseVertex;
	result.NumVertices = NumVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushCapsule(uint16 slices, uint16 rows, float height, float radius) {
	AlignNextTriangle();

	uint32 baseVertex = NumVertices;
	uint32 firstTriangle = NumTriangles;

	assert(slices > 2);
	assert(rows > 0);
	assert(rows % 2 == 1);

	float vertDeltaAngle = M_PI / (rows + 1);
	float horzDeltaAngle = 2.f * M_PI / slices;
	float halfHeight = 0.5f * height;
	float texStretch = radius / (radius + halfHeight);

	assert(slices * (rows + 1) + 2 <= UINT16_MAX);

	Reserve(slices * (rows + 1) + 2, 2 * (rows + 1) * slices);

	uint8* vertexPtr = Vertices + VertexSize * NumVertices;

	// Vertices.
	PushVertex(vec3(0.f, -radius - halfHeight, 0.f), DirectionToPanoramaUv(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	for (uint32 y = 0; y < rows / 2u + 1u; y++) {
		float vertAngle = (y + 1) * vertDeltaAngle - M_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; x++) {
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius - halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);

			vec2 uv = DirectionToPanoramaUv(nor);
			uv.y *= texStretch;
			PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}
	for (uint32 y = 0; y < rows / 2u + 1u; y++) {
		float vertAngle = (y + rows / 2 + 1) * vertDeltaAngle - M_PI;
		float vertexY = cosf(vertAngle);
		float currentCircleRadius = sinf(vertAngle);
		for (uint32 x = 0; x < slices; x++) {
			float horzAngle = x * horzDeltaAngle;
			float vertexX = cosf(horzAngle) * currentCircleRadius;
			float vertexZ = sinf(horzAngle) * currentCircleRadius;
			vec3 pos(vertexX * radius, vertexY * radius + halfHeight, vertexZ * radius);
			vec3 nor(vertexX, vertexY, vertexZ);

			vec2 uv = DirectionToPanoramaUv(nor);
			uv.y *= texStretch;
			PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}
	PushVertex(vec3(0.f, radius + halfHeight, 0.f), DirectionToPanoramaUv(vec3(0.f, 1.f, 0.f)), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	uint16 lastVertex = slices * (rows + 1) + 2;

	uint32 triIndex = 0;

	// Indices.
	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(0, x + 1, x + 2);
	}
	PushTriangle(0, slices, 1);
	for (uint32 y = 0; y < rows; y++) {
		for (uint32 x = 0; x < slices - 1u; x++) {
			PushTriangle(y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x);
			PushTriangle(y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x);
		}
		PushTriangle(y * slices + slices, (y + 1) * slices + 1, y * slices + 1);
		PushTriangle(y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1);
	}
	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(lastVertex - 2 - x, lastVertex - 3 - x, lastVertex - 1);
	}
	PushTriangle(lastVertex - 1 - slices, lastVertex - 2, lastVertex - 1);

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 2 * (rows + 1) * slices;
	result.BaseVertex = baseVertex;
	result.NumVertices = NumVertices - baseVertex;
	return result;
}

DxMesh CpuMesh::CreateDxMesh() const {
	DxMesh result;
	result.VertexBuffer = DxVertexBuffer::Create(VertexSize, NumVertices, Vertices);
	result.IndexBuffer = DxIndexBuffer::Create(sizeof(uint16), NumTriangles * 3, Triangles);
	return result;
}

#define GetVertexProperty(prop, base, info, type) *(type*)(base + info.prop##Offset)

DxVertexBuffer CpuMesh::CreateVertexBufferWithAlternativeLayout(DxContext* context, uint32 otherFlags, bool allowUnorderedAccess) {
#ifdef _DEBUG
	for (uint32 i = 0; i < 31; i++) {
		uint32 testFlag = (1 << i);
		if (otherFlags & testFlag) {
			assert(Flags & testFlag); // We can only remove flags, not set new flags
		}
	}
#endif
	VertexInfo ownInfo = GetVertexInfo(Flags);
	VertexInfo newInfo = GetVertexInfo(otherFlags);

	uint8* newVertices = (uint8*)malloc(newInfo.VertexSize * NumVertices);

	for (uint32 i = 0; i < NumVertices; i++) {
		uint8* ownBase = Vertices + i * ownInfo.VertexSize;
		uint8* newBase = newVertices + i * newInfo.VertexSize;

		if (otherFlags & EMeshCreationFlagsWithPositions) {
			GetVertexProperty(Position, newBase, newInfo, vec3) = GetVertexProperty(Position, ownBase, ownInfo, vec3);
		}
		if (otherFlags & EMeshCreationFlagsWithUvs) {
			GetVertexProperty(Uv, newBase, newInfo, vec2) = GetVertexProperty(Uv, ownBase, ownInfo, vec2);
		}
		if (otherFlags & EMeshCreationFlagsWithNormals) {
			GetVertexProperty(Normal, newBase, newInfo, vec3) = GetVertexProperty(Normal, ownBase, ownInfo, vec3);
		}
		if (otherFlags & EMeshCreationFlagsWithTangents) {
			GetVertexProperty(Tangent, newBase, newInfo, vec3) = GetVertexProperty(Tangent, ownBase, ownInfo, vec3);
		}
		if (otherFlags & EMeshCreationFlagsWithSkin) {
			GetVertexProperty(Skin, newBase, newInfo, SkinningWeights) = GetVertexProperty(Skin, ownBase, ownInfo, SkinningWeights);
		}
	}

	DxVertexBuffer vertexBuffer = DxVertexBuffer::Create(newInfo.VertexSize, NumVertices, newVertices, allowUnorderedAccess);
	free(newVertices);
	return vertexBuffer;
}

#undef GetVertexProperty
#undef PushVertex

