#pragma once

#include "../directx/DxRenderPrimitives.h"
#include "colliders.h"
#include "../animation/animation.h"

// Members are always pushed in this order!
enum MeshCreationFlags {
	EMeshCreationFlagsNone			= 0,
	EMeshCreationFlagsWithPositions = (1 << 0),
	EMeshCreationFlagsWithUvs		= (1 << 1),
	EMeshCreationFlagsWithNormals	= (1 << 2),
	EMeshCreationFlagsWithTangents	= (1 << 3),
	EMeshCreationFlagsWithSkin		= (1 << 4),
};

class CpuMesh {
public:
	CpuMesh() {}
	CpuMesh(uint32 flags);
	CpuMesh(const CpuMesh&) = delete;
	CpuMesh(CpuMesh&& mesh);
	~CpuMesh();

	uint32 Flags;
	uint32 VertexSize;
	uint32 SkinOffset;

	uint8* Vertices;
	IndexedTriangle16* Triangles;

	uint32 NumVertices;
	uint32 NumTriangles;

	SubmeshInfo PushQuad(vec2 radius);
	SubmeshInfo PushQuad(float radius) {
		return PushQuad(vec2(radius, radius));
	}
	SubmeshInfo PushCube(vec3 radius, bool flipWindingOrder = false);
	SubmeshInfo PushCube(float radius, bool flipWindingOrder = false) {
		return PushCube(vec3(radius, radius, radius), flipWindingOrder);
	}
	SubmeshInfo PushSphere(uint16 slices, uint16 rows, float radius);
	SubmeshInfo PushCapsule(uint16 slices, uint16 rows, float height, float radius);

	DxMesh CreateDxMesh();
	DxVertexBuffer CreateVertexBufferWithAlternativeLayout(DxContext* context, uint32 otherFlags, bool allowUnorderedAccess = false);

private:
	void AlignNextTriangle();
	void Reserve(uint32 vertexCount, uint32 triangleCount);
	void PushTriangle(uint16 a, uint16 b, uint16 c);
};