#include "../pch.h"
#include "geometry.h"
#include "../core/memory.h"

#include "assimp/scene.h"
#include <unordered_map>

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
	_vertices = mesh._vertices;
	_triangles = mesh._triangles;
	_numVertices = mesh._numVertices;
	_numTriangles = mesh._numTriangles;

	mesh._vertices = 0;
	mesh._triangles = 0;
}

CpuMesh::~CpuMesh() {
	if (_vertices) {
		_aligned_free(_vertices);
	}
	if (_triangles) {
		_aligned_free(_triangles);
	}
}

void CpuMesh::AlignNextTriangle() {
	// This is called when a new mesh is pushed. The function aligns the next index to a 16-byte boundary.
	_numTriangles = AlignTo(_numTriangles, 8); // 8 triangles are 48 bytes, which is divisible by 16.
}

void CpuMesh::Reserve(uint32 vertexCount, uint32 triangleCount) {
	_vertices = (uint8*)_aligned_realloc(_vertices, (_numVertices + vertexCount) * VertexSize, 64);
	_triangles = (TriangleT*)_aligned_realloc(_triangles, (_numTriangles + triangleCount + 8) * sizeof(TriangleT), 64); // Allocate 8 more, such that we can align without problems.
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
	uint8* ptrVertex = _vertices + VertexSize * _numVertices;
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
	++_numVertices;
}


void CpuMesh::PushTriangle(IndexT a, IndexT b, IndexT c) {
	_triangles[_numTriangles++] = { a, b, c };
}

SubmeshInfo CpuMesh::PushQuad(vec2 radius) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	Reserve(4, 2);

	uint8* vertexPtr = _vertices + VertexSize * _numVertices;

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

SubmeshInfo CpuMesh::PushCube(vec3 radius, bool flipWindingOrder, vec3 center) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	if ((Flags & EMeshCreationFlagsWithPositions)
		and not(Flags & EMeshCreationFlagsWithUvs)
		and not(Flags & EMeshCreationFlagsWithNormals)
		and not(Flags & EMeshCreationFlagsWithTangents)) {
		Reserve(8, 12);

		PushVertex(center + vec3(-radius.x, -radius.y, radius.z), {}, {}, {}, {});  // 0
		PushVertex(center + vec3(radius.x, -radius.y, radius.z), {}, {}, {}, {});   // x
		PushVertex(center + vec3(-radius.x, radius.y, radius.z), {}, {}, {}, {});   // y
		PushVertex(center + vec3(radius.x, radius.y, radius.z), {}, {}, {}, {});	// xy
		PushVertex(center + vec3(-radius.x, -radius.y, -radius.z), {}, {}, {}, {}); // z
		PushVertex(center + vec3(radius.x, -radius.y, -radius.z), {}, {}, {}, {});  // xz
		PushVertex(center + vec3(-radius.x, radius.y, -radius.z), {}, {}, {}, {});  // yz
		PushVertex(center + vec3(radius.x, radius.y, -radius.z), {}, {}, {}, {});   // xyz

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

		uint8* vertexPtr = _vertices + VertexSize * _numVertices;

		PushVertex(center + vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, 1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, -radius.y, radius.z), vec2(0.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, radius.y, radius.z), vec2(0.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 0.f, -1.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, -radius.y, radius.z), vec2(1.f, 0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, radius.y, radius.z), vec2(1.f, 1.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, radius.y, radius.z), vec2(0.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(center + vec3(radius.x, radius.y, radius.z), vec2(1.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, radius.y, -radius.z), vec2(0.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(center + vec3(radius.x, radius.y, -radius.z), vec2(1.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, -radius.y, -radius.z), vec2(0.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(center + vec3(radius.x, -radius.y, -radius.z), vec2(1.f, 0.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(center + vec3(-radius.x, -radius.y, radius.z), vec2(0.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});
		PushVertex(center + vec3(radius.x, -radius.y, radius.z), vec2(1.f, 1.f), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

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
		for (uint32 i = _numTriangles - 12; i < _numTriangles; i++) {
			IndexT tmp = _triangles[i].B;
			_triangles[i].B = _triangles[i].C;
			_triangles[i].C = tmp;
		}
	}

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 12;
	result.BaseVertex = baseVertex;
	result.NumVertices = _numVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushSphere(uint16 slices, uint16 rows, float radius) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	assert(slices > 2);
	assert(rows > 0);

	float vertDeltaAngle = PI / (rows + 1);
	float horzDeltaAngle = 2.f * PI / slices;

	if (sizeof(IndexT) == 2) {
		assert(slices * rows + 2 == UINT16_MAX);
	}

	Reserve(slices * rows + 2, 2 * rows * slices);

	// Vertices
	PushVertex(vec3(0.f, -radius, 0.f), DirectionToPanoramaUv(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	for (uint32 y = 0; y < rows; y++) {
		float vertAngle = (y + 1) * vertDeltaAngle - PI;
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

	IndexT lastVertex = slices * rows + 2;
	
	// Indices.
	for (IndexT x = 0; x < slices - 1u; x++) {
		PushTriangle(0, x + 1u, x + 2u);
	}
	PushTriangle(0, slices, 1);

	for (IndexT y = 0; y < rows - 1u; y++) {
		for (IndexT x = 0; x < slices - 1u; x++) {
			PushTriangle(y * slices + 1u + x, (y + 1u) * slices + 2u + x, y * slices + 2u + x);
			PushTriangle(y * slices + 1u + x, (y + 1u) * slices + 1u + x, (y + 1u) * slices + 2u + x);
		}
		PushTriangle((IndexT)(y * slices + slices), (IndexT)((y + 1u) * slices + 1u), (IndexT)(y * slices + 1u));
		PushTriangle((IndexT)(y * slices + slices), (IndexT)((y + 1u) * slices + slices), (IndexT)((y + 1u) * slices + 1u));
	}
	for (IndexT x = 0; x < slices - 1; x++) {
		PushTriangle(lastVertex - 2u - x, lastVertex - 3u - x, lastVertex - 1u);
	}
	PushTriangle(lastVertex - 1u - slices, lastVertex - 2u, lastVertex - 1u);

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 2 * rows * slices;
	result.BaseVertex = baseVertex;
	result.NumVertices = _numVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushIcoSphere(float radius, uint32 refinement) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	struct Vert {
		vec3 P;
		vec3 N;
		vec3 T;
	};

	std::vector<Vert> vertices;
	std::vector<TriangleT> triangles;

	float t = (1.f + sqrt(5.f)) / 2.f;

	#define PushIcoVertex(p) { vec3 nor = normalize(p); vec3 px = nor * radius; vec3 tan = normalize(cross(vec3(0.f, 1.f, 0.f), nor)); vertices.push_back({px, nor, tan}); }

	PushIcoVertex(vec3(-1.f, t, 0));
	PushIcoVertex(vec3(1.f, t, 0));
	PushIcoVertex(vec3(-1.f, -t, 0));
	PushIcoVertex(vec3(1.f, -t, 0));

	PushIcoVertex(vec3(0, -1.f, t));
	PushIcoVertex(vec3(0, 1.f, t));
	PushIcoVertex(vec3(0, -1.f, -t));
	PushIcoVertex(vec3(0, 1.f, -t));

	PushIcoVertex(vec3(t, 0, -1.f));
	PushIcoVertex(vec3(t, 0, 1.f));
	PushIcoVertex(vec3(-t, 0, -1.f));
	PushIcoVertex(vec3(-t, 0, 1.f));

	triangles.push_back({ 0, 11, 5 });
	triangles.push_back({ 0, 5, 1 });
	triangles.push_back({ 0, 1, 7 });
	triangles.push_back({ 0, 7, 10 });
	triangles.push_back({ 0, 10, 11 });
	triangles.push_back({ 1, 5, 9 });
	triangles.push_back({ 5, 11, 4 });
	triangles.push_back({ 11, 10, 2 });
	triangles.push_back({ 10, 7, 6 });
	triangles.push_back({ 7, 1, 8 });
	triangles.push_back({ 3, 9, 4 });
	triangles.push_back({ 3, 4, 2 });
	triangles.push_back({ 3, 2, 6 });
	triangles.push_back({ 3, 6, 8 });
	triangles.push_back({ 3, 8, 9 });
	triangles.push_back({ 4, 9, 5 });
	triangles.push_back({ 2, 4, 11 });
	triangles.push_back({ 6, 2, 10 });
	triangles.push_back({ 8, 6, 7 });
	triangles.push_back({ 9, 8, 1 });

	std::unordered_map<uint32, IndexT> midpoints;

	auto getMiddlePoint = [&midpoints, &vertices, radius](uint32 a, uint32 b) {
		uint32 edge = (Min(a, b) << 16) | (Max(a, b));
		auto it = midpoints.find(edge);
		if (it == midpoints.end()) {
			vec3 point1 = vertices[a].P;
			vec3 point2 = vertices[b].P;

			vec3 center = 0.5f * (point1 + point2);
			PushIcoVertex(center);

			IndexT index = (IndexT)vertices.size() - 1;

			midpoints.insert({ edge, index });
			return index;
		}

		return it->second;
	};

	for (uint32 r = 0; r < refinement; ++r) {
		std::vector<TriangleT> refinedTriangles;

		for (uint32 tri = 0; tri < (uint32)triangles.size(); ++tri) {
			TriangleT& t = triangles[tri];

			IndexT a = getMiddlePoint(t.A, t.B);
			IndexT b = getMiddlePoint(t.B, t.C);
			IndexT c = getMiddlePoint(t.C, t.A);

			refinedTriangles.push_back({ t.A, a, c });
			refinedTriangles.push_back({ t.B, b, a });
			refinedTriangles.push_back({ t.C, c, b });
			refinedTriangles.push_back({ a, b, c });
		}

		triangles = refinedTriangles;
	}

	Reserve((uint32)vertices.size(), (uint32)triangles.size());

	uint8* vertexPtr = _vertices + VertexSize * _numVertices;
	for (const Vert& v : vertices) {
		PushVertex(v.P, {}, v.N, v.T, {});
	}

	for (TriangleT t : triangles) {
		PushTriangle(t.A, t.B, t.C);
	}

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = (uint32)triangles.size();
	result.BaseVertex = baseVertex;
	result.NumVertices = (uint32)vertices.size();
	return result;

#undef PushIcoVertex
}


SubmeshInfo CpuMesh::PushCapsule(uint16 slices, uint16 rows, float height, float radius) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	assert(slices > 2);
	assert(rows > 0);
	assert(rows % 2 == 1);

	float vertDeltaAngle = PI / (rows + 1);
	float horzDeltaAngle = 2.f * PI / slices;
	float halfHeight = 0.5f * height;
	float texStretch = radius / (radius + halfHeight);

	if (sizeof(IndexT) == 2) {
		assert(slices * (rows + 1) + 2 <= UINT16_MAX);
	}

	Reserve(slices * (rows + 1) + 2, 2 * (rows + 1) * slices);

	uint8* vertexPtr = _vertices + VertexSize * _numVertices;

	// Vertices.
	PushVertex(vec3(0.f, -radius - halfHeight, 0.f), DirectionToPanoramaUv(vec3(0.f, -1.f, 0.f)), vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	for (uint32 y = 0; y < rows / 2u + 1u; y++) {
		float vertAngle = (y + 1) * vertDeltaAngle - PI;
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
			PushVertex(pos, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}
	for (uint32 y = 0; y < rows / 2u + 1u; y++) {
		float vertAngle = (y + rows / 2 + 1) * vertDeltaAngle - PI;
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
			PushVertex(pos, uv, normalize(nor), normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
		}
	}
	PushVertex(vec3(0.f, radius + halfHeight, 0.f), DirectionToPanoramaUv(vec3(0.f, 1.f, 0.f)), vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	IndexT lastVertex = slices * (rows + 1) + 2;

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
	result.NumVertices = _numVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushCylinder(uint16 slices, float radius, float height) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	assert(slices > 2);

	float horzDeltaAngle = 2.f * PI / slices;
	float halfHeight = height * 0.5f;

	Reserve(4 * slices + 2, 4 * slices);

	vec2 uv(0.f, 0.f);
	PushVertex(vec3(0.f, -halfHeight, 0.f), uv, vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	// Bottom row, normal down.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * radius, -halfHeight, vertexZ * radius);
		vec3 nor(0.f, -1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Bottom row, normal around
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * radius, -halfHeight, vertexZ * radius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal around
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = cosf(horzAngle);
		vec3 pos(vertexX * radius, halfHeight, vertexZ * radius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal up.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * radius, halfHeight, vertexZ * radius);
		vec3 nor(0.f, 1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	PushVertex(vec3(0.f, halfHeight, 0.f), uv, vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	IndexT lastVertex = 4 * slices + 2;

	// Indices.
	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(0, x + 1, x + 2);
	}
	PushTriangle(0, slices, 1);

	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(slices + 1 + x, 2 * slices + 2 + x, slices + 2 + x);
		PushTriangle(slices + 1 + x, 2 * slices + 1 + x, 2 * slices + 2 + x);
	}
	PushTriangle(slices + slices, 2 * slices + 1, slices + 1);
	PushTriangle(slices + slices, 2 * slices + slices, 2 * slices + 1);

	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(lastVertex - 2 - x, lastVertex - 3 - x, lastVertex - 1);
	}
	PushTriangle(lastVertex - 1 - slices, lastVertex - 2, lastVertex - 1);

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 4 * slices;
	result.BaseVertex = baseVertex;
	result.NumVertices = _numVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushArrow(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	assert(slices > 2);

	float horzDeltaAngle = 2.f * PI / slices;

	Reserve(7 * slices + 1, 7 * slices);

	vec2 uv(0.f, 0.f);
	PushVertex(vec3(0.f, 0.f, 0.f), uv, vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	// Bottom row, normal down
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Bottom row, normal around
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal around
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal down
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top outer row, normal down
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(0.f, -1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	vec2 normal2D = normalize(vec2(headLength, headRadius));

	// Top outer row, normal around
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(vertexX * normal2D.x, normal2D.y, vertexZ * normal2D.x);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top vertex.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(0.f, shaftLength + headLength, 0.f);
		vec3 nor(vertexX * normal2D.x, normal2D.y, vertexZ * normal2D.x);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Indices
	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(0, x + 1, x + 2);
	}
	PushTriangle(0, slices, 1);

	for (uint32 y = 1; y < 7; y += 2) {
		for (uint32 x = 0; x < slices - 1u; x++) {
			PushTriangle(y * slices + 1 + x, (y + 1) * slices + 2 + x, y * slices + 2 + x);
			PushTriangle(y * slices + 1 + x, (y + 1) * slices + 1 + x, (y + 1) * slices + 2 + x);
		}
		PushTriangle(y * slices + slices, (y + 1) * slices + 1, y * slices + 1);
		PushTriangle(y * slices + slices, (y + 1) * slices + slices, (y + 1) * slices + 1);
	}

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = 7 * slices;
	result.BaseVertex = baseVertex;
	result.NumVertices = _numVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushTorus(uint16 slices, uint16 segments, float torusRadius, float tubeRadius) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	assert(slices > 2);
	assert(slices > 2);

	float tubeDeltaAngle = 2.f * PI / slices;
	float torusDeltaAngle = 2.f * PI / segments;

	Reserve(segments * slices, segments * slices * 2);

	vec2 uv(0.f, 0.f);

	quat torusRotation(vec3(1.f, 0.f, 0.f), deg2rad(90.f));

	for (uint32 s = 0; s < segments; s++) {
		float segmentAngle = s * torusDeltaAngle;
		quat segmentRotation(vec3(0.f, 0.f, 1.f), segmentAngle);

		vec3 segmentOffset = segmentRotation * vec3(torusRadius, 0.f, 0.f);

		for (uint32 x = 0; x < slices; x++) {
			float horzAngle = x * tubeDeltaAngle;
			float vertexX = cosf(horzAngle);
			float vertexZ = sinf(horzAngle);
			vec3 pos = torusRotation * (segmentRotation * vec3(vertexX * tubeRadius, 0.f, vertexZ * tubeRadius) + segmentOffset);
			vec3 nor = torusRotation * (segmentRotation * vec3(vertexX, 0.f, vertexZ));

			PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
		}
	}

	IndexT _numVertices = segments * slices;

	for (uint32 y = 0; y < segments - 1u; y++) {
		for (uint32 x = 0; x < slices - 1u; x++) {
			PushTriangle(y * slices + x, (y + 1) * slices + 1 + x, y * slices + 1 + x);
			PushTriangle(y * slices + x, (y + 1) * slices + x, (y + 1) * slices + 1 + x);
		}
		PushTriangle(y * slices + slices - 1, (y + 1) * slices, y * slices);
		PushTriangle(y * slices + slices - 1, (y + 1) * slices + slices - 1, (y + 1) * slices);
	}

	uint32 y = segments - 1u;
	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(y * slices + x, 1 + x, y * slices + 1 + x);
		PushTriangle(y * slices + x, x, 1 + x);
	}
	PushTriangle(y * slices + slices - 1, 0, y * slices);
	PushTriangle(y * slices + slices - 1, slices - 1, 0);

	SubmeshInfo result;
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = segments * slices * 2;
	result.BaseVertex = baseVertex;
	result.NumVertices = _numVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushMace(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	assert(slices > 2);

	float horzDeltaAngle = 2.f * PI / slices;

	Reserve(8 * slices + 2, 8 * slices);

	vec2 uv(0.f, 0.f);
	PushVertex(vec3(0.f, 0.f, 0.f), uv, vec3(0.f, -1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	// Bottom row, normal down.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Bottom row, normal around.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, 0.f, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal around.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, normalize(cross(vec3(0.f, 1.f, 0.f), nor)), {});
	}

	// Top row, normal down.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * shaftRadius, shaftLength, vertexZ * shaftRadius);
		vec3 nor(0.f, -1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top outer row, normal down.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(0.f, -1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top outer row, normal around.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength, vertexZ * headRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top top outer row, normal around.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength + headLength, vertexZ * headRadius);
		vec3 nor(vertexX, 0.f, vertexZ);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	// Top top outer row, normal up.
	for (uint32 x = 0; x < slices; x++) {
		float horzAngle = x * horzDeltaAngle;
		float vertexX = cosf(horzAngle);
		float vertexZ = sinf(horzAngle);
		vec3 pos(vertexX * headRadius, shaftLength + headLength, vertexZ * headRadius);
		vec3 nor(0.f, 1.f, 0.f);

		PushVertex(pos, uv, nor, vec3(1.f, 0.f, 0.f), {});
	}

	PushVertex(vec3(0.f, shaftLength + headLength, 0.f), uv, vec3(0.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), {});

	IndexT lastVertex = 8 * slices + 2;

	// Indices.
	for (uint32 x = 0; x < slices - 1u; x++) {
		PushTriangle(0, x + 1, x + 2);
	}
	PushTriangle(0, slices, 1);

	for (uint32 y = 1; y < 7; y += 2) {
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
	result.NumTriangles = 8 * slices;
	result.BaseVertex = baseVertex;
	result.NumVertices = _numVertices - baseVertex;
	return result;
}

SubmeshInfo CpuMesh::PushAssimpMesh(const struct aiMesh* mesh, float scale, BoundingBox *aabb, AnimationSkeleton* skeleton) {
	AlignNextTriangle();

	uint32 baseVertex = _numVertices;
	uint32 firstTriangle = _numTriangles;

	if (sizeof(IndexT) == 2) {
		assert(mesh->mNumVertices <= UINT16_MAX);
	}

	Reserve(mesh->mNumVertices, mesh->mNumFaces);
	vec3 position(0.f, 0.f, 0.f);
	vec3 normal(0.f,0.f,0.f);
	vec3 tangent(0.f,0.f,0.f);
	vec2 uv(0.f,0.f);

	const bool hasPositions = mesh->HasPositions();
	const bool hasNormals = mesh->HasNormals();
	const bool hasTangents = mesh->HasTangentsAndBitangents();
	const bool hasUVs = mesh->HasTextureCoords(0);

	if (aabb) {
		*aabb = BoundingBox::NegativeInfinity();
	}

	for (uint32 i = 0; i < mesh->mNumVertices; i++) {
		if (hasPositions) {
			position = vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z) * scale;
			if (aabb) {
				aabb->Grow(position);
			}
		}
		if (hasNormals) {
			normal = vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		}
		if (hasTangents) {
			tangent = vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
		}
		if (hasUVs) {
			uv = vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}

		PushVertex(position, uv, normal, tangent, {});
	}

	if ((Flags & EMeshCreationFlagsWithSkin) and skeleton) {
		assert(mesh->HasBones());

		uint32 numBones = mesh->mNumBones;

		assert(numBones < 256);

		for (uint32 boneID = 0; boneID < numBones; ++boneID) {
			const aiBone* bone = mesh->mBones[boneID];

			auto it = skeleton->NameToJointId.find(bone->mName.C_Str());
			assert(it != skeleton->NameToJointId.end());

			uint8 jointID = (uint8)it->second;

			for (uint32 weightID = 0; weightID < bone->mNumWeights; ++weightID) {
				uint32 vertexID = bone->mWeights[weightID].mVertexId;
				float weight = bone->mWeights[weightID].mWeight;

				assert(vertexID < mesh->mNumVertices);
				assert(vertexID + baseVertex < _numVertices);

				vertexID += baseVertex;
				uint8* vertexBase = _vertices + (vertexID * VertexSize);
				SkinningWeights& weights = *(SkinningWeights*)(vertexBase + SkinOffset);

				bool foundFreeSlot = false;
				for (uint32 i = 0; i < 4; ++i) {
					if (weights.SkinWeights[i] == 0) {
						weights.SkinIndices[i] = jointID;
						weights.SkinWeights[i] = (uint8)clamp(weight * 255.f, 0.f, 255.f);
						foundFreeSlot = true;
						break;
					}
				}
				if (!foundFreeSlot) {
					assert(!"Mesh has more than 4 weights per vertex.");
				}
			}
		}

		for (uint32 i = 0; i < mesh->mNumVertices; ++i) {
			uint8* vertexBase = _vertices + ((i + baseVertex) * VertexSize);
			SkinningWeights& weights = *(SkinningWeights*)(vertexBase + SkinOffset);

			assert(weights.SkinWeights[0] > 0);

			vec4 v = { (float)weights.SkinWeights[0], (float)weights.SkinWeights[1], (float)weights.SkinWeights[2], (float)weights.SkinWeights[3] };
			v /= 255.f;

			float sum = dot(v, 1.f);
			if (abs(sum - 1.f) >= 0.05f) {
				int a = 0;
			}
		}
	}

	for (uint32 i = 0; i < mesh->mNumFaces; i++) {
		const aiFace& face = mesh->mFaces[i];
		PushTriangle(face.mIndices[0], face.mIndices[1], face.mIndices[2]);
	}

	SubmeshInfo result = {};
	result.FirstTriangle = firstTriangle;
	result.NumTriangles = mesh->mNumFaces;
	result.BaseVertex = baseVertex;
	result.NumVertices = mesh->mNumVertices;

	return result;
}


DxMesh CpuMesh::CreateDxMesh() const {
	DxMesh result;
	result.VertexBuffer = DxVertexBuffer::Create(VertexSize, _numVertices, _vertices);
	result.IndexBuffer = DxIndexBuffer::Create(sizeof(IndexT), _numTriangles * 3, _triangles);
	return result;
}

#define GetVertexProperty(prop, base, info, type) *(type*)(base + info.prop##Offset)

Ptr<DxVertexBuffer> CpuMesh::CreateVertexBufferWithAlternativeLayout(uint32 otherFlags, bool allowUnorderedAccess) const {
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

	uint8* newVertices = (uint8*)malloc(newInfo.VertexSize * _numVertices);

	for (uint32 i = 0; i < _numVertices; i++) {
		uint8* ownBase = _vertices + i * ownInfo.VertexSize;
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

	Ptr<DxVertexBuffer> vertexBuffer = DxVertexBuffer::Create(newInfo.VertexSize, _numVertices, newVertices, allowUnorderedAccess);
	free(newVertices);
	return vertexBuffer;
}

#undef GetVertexProperty
