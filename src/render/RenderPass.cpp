#include "RenderPass.h"
#include "../core/camera.h"
#include "../directx/DxRenderer.h"
#include "../directx/DxContext.h"

void GeometryRenderPass::Reset() {
    _drawCalls.clear();
    _outlinedObjects.clear();
}

void OpaqueRenderPass::Reset() {
    GeometryRenderPass::Reset();

    _staticDepthOnlyDrawCalls.clear();
    _dynamicDepthOnlyDrawCalls.clear();
    _animatedDepthOnlyDrawCalls.clear();
}

void SunShadowRenderPass::RenderObject(uint32 cascadeIndex, const Ptr<DxVertexBuffer> &vertexBuffer, const Ptr<DxIndexBuffer> &indexBuffer, SubmeshInfo submesh, const mat4 &transform) {
    _drawCalls[cascadeIndex].push_back({
        transform,
        vertexBuffer,
        indexBuffer,
        submesh
    });
}

void SunShadowRenderPass::Reset() {
    for (uint32 i = 0; i < std::size(_drawCalls); i++) {
        _drawCalls[i].clear();
    }
}

void SpotShadowRenderPass::RenderObject(const Ptr<DxVertexBuffer> &vertexBuffer, const Ptr<DxIndexBuffer> &indexBuffer, SubmeshInfo submesh, const mat4 &transform) {
    _drawCalls.push_back({
        transform,
        vertexBuffer,
        indexBuffer,
        submesh
    });
}

void SpotShadowRenderPass::Reset() {
    _drawCalls.clear();
}

void PointShadowRenderPass::RenderObject(const Ptr<DxVertexBuffer> &vertexBuffer, const Ptr<DxIndexBuffer> &indexBuffer, SubmeshInfo submesh, const mat4 &transform) {
    _drawCalls.push_back({
        transform,
        vertexBuffer,
        indexBuffer,
        submesh
    });
}

void PointShadowRenderPass::Reset() {
    _drawCalls.clear();
}





