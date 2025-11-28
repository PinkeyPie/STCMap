#pragma once

#include "../directx/DxBuffer.h"
#include "../physics/geometry.h"
#include "../core/math.h"

struct VertexRange {
    uint32 FirstVertex;
    uint32 NumVertices;
};

void InitializeSkinning();
std::tuple<Ptr<DxVertexBuffer>, VertexRange, mat4*> SkinObject(const Ptr<DxVertexBuffer>& vertexBuffer, VertexRange range, uint32 numJoints);
std::tuple<Ptr<DxVertexBuffer>, uint32, mat4*> SkinObject(const Ptr<DxVertexBuffer>& vertexBuffer, uint32 numJoints);
std::tuple<Ptr<DxVertexBuffer>, SubmeshInfo, mat4*> SkinObject(const Ptr<DxVertexBuffer>& vertexBuffer, SubmeshInfo submesh, uint32 numJoints);
bool PerformSkinning();