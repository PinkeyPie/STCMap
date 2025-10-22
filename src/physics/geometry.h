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

	uint32 Flags = 0;
	uint32 VertexSize = 0;
	uint32 SkinOffset = 0;

	uint8* Vertices = nullptr;
	IndexedTriangle16* Triangles = nullptr;

	uint32 NumVertices = 0;
	uint32 NumTriangles = 0;

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

	SubmeshInfo PushAssimpMesh(const struct aiMesh* mesh, float scale, AABBCollider* aabb = nullptr);

	DxMesh CreateDxMesh() const;
	DxVertexBuffer CreateVertexBufferWithAlternativeLayout(uint32 otherFlags, bool allowUnorderedAccess = false) const;

private:
	void AlignNextTriangle();
	void Reserve(uint32 vertexCount, uint32 triangleCount);
	void PushTriangle(uint16 a, uint16 b, uint16 c);
	void PushVertex(vec3 position, vec2 uv, vec3 normal, vec3 tangent, SkinningWeights skin);
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPosition[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
};

static D3D12_INPUT_ELEMENT_DESC inputLayoutPositionUv[] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	{"TEXCOORDS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
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