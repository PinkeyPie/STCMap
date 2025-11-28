#pragma once

#include "bounding_volumes.h"
#include "../animation/animation.h"
#include "../directx/DxBuffer.h"

class SubmeshInfo {
public:
	uint32 NumTriangles;
	uint32 FirstTriangle;
	uint32 BaseVertex;
	uint32 NumVertices;
};

// Members are always pushed in this order!
enum MeshCreationFlags {
	EMeshCreationFlagsNone			= 0,
	EMeshCreationFlagsWithPositions = (1 << 0),
	EMeshCreationFlagsWithUvs		= (1 << 1),
	EMeshCreationFlagsWithNormals	= (1 << 2),
	EMeshCreationFlagsWithTangents	= (1 << 3),
	EMeshCreationFlagsWithSkin		= (1 << 4),
};

static uint32 GetVertexSize(uint32 meshFlags) {
	uint32 size = 0;
	if (meshFlags & EMeshCreationFlagsWithPositions) {
		size += sizeof(vec3);
	}
	if (meshFlags & EMeshCreationFlagsWithUvs) {
		size += sizeof(vec2);
	}
	if (meshFlags & EMeshCreationFlagsWithNormals) {
		size += sizeof(vec3);
	}
	if (meshFlags & EMeshCreationFlagsWithTangents) {
		size += sizeof(vec3);
	}
	if (meshFlags & EMeshCreationFlagsWithSkin) {
		size += sizeof(SkinningWeights);
	}
	return size;
}

class CpuMesh {
public:
	using TriangleT = IndexedLine32;

	CpuMesh() = default;
	CpuMesh(uint32 flags);
	CpuMesh(const CpuMesh&) = delete;
	CpuMesh(CpuMesh&& mesh);
	~CpuMesh();

	uint32 Flags = 0;
	uint32 VertexSize = 0;
	uint32 SkinOffset = 0;

	SubmeshInfo PushQuad(vec2 radius);
	SubmeshInfo PushQuad(float radius) { return PushQuad(vec2(radius, radius)); }
	SubmeshInfo PushCube(vec3 radius, bool flipWindingOrder = false, vec3 center = vec3(0.f, 0.f, 0.f));
	SubmeshInfo PushCube(float radius, bool flipWindingOrder = false, vec3 center = vec3(0.f, 0.f, 0.f)) { return PushCube(vec3(radius, radius, radius), flipWindingOrder, center); }
	SubmeshInfo PushSphere(uint16 slices, uint16 rows, float radius);
	SubmeshInfo PushIcoSphere(float radius, uint32 refinement);
	SubmeshInfo PushCapsule(uint16 slices, uint16 rows, float height, float radius);
	SubmeshInfo PushCylinder(uint16 slices, float radius, float height);
	SubmeshInfo PushArrow(uint16 slices, float shaftRadius, float headRadius, float shaftLength, float headLength);
	SubmeshInfo PushTorus(uint16 slices, uint16 segments, float torusRadius, float tubeRadius);
	SubmeshInfo PushMace(uint16 slices, float shaftRadius, float headRadius, float shiftLength, float headLength);

	SubmeshInfo PushAssimpMesh(const struct aiMesh* mesh, float scale, BoundingBox* aabb = nullptr, AnimationSkeleton* skeleton = nullptr);

	DxMesh CreateDxMesh() const;
	Ptr<DxVertexBuffer> CreateVertexBufferWithAlternativeLayout(uint32 otherFlags, bool allowUnorderedAccess = false) const;

private:
	using IndexT = decltype(TriangleT::A);

	uint8* _vertices = nullptr;
	TriangleT* _triangles = nullptr;

	uint32 _numVertices = 0;
	uint32 _numTriangles = 0;

	void AlignNextTriangle();
	void Reserve(uint32 vertexCount, uint32 triangleCount);
	void PushTriangle(IndexT a, IndexT b, IndexT c);
	void PushVertex(vec3 position, vec2 uv, vec3 normal, vec3 tangent, SkinningWeights skin);
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPosition[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPositionUv[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPositionNormal[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPositionUvNormal[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPositionUvNormalTangent[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPositionUvNormalSkin[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"SKIN_INDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"SKIN_WEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPositionUvNormalTangentSkin[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"SKIN_INDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"SKIN_WEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};