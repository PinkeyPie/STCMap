#pragma once

#include "../core/math.h"
#include "../physics/bounding_volumes.h"
#include "../physics/mesh.h"
#include "LightSource.h"
#include "material.h"

struct RaytracingTlas;
class DxVertexBuffer;
class DxIndexBuffer;
class RaytracingBlas;

// Base for both opaque and transparent pass
class GeometryRenderPass {
public:
    void Reset();
protected:
    template<bool opaque, class MaterialT>
    void Common(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh, const Ptr<MaterialT>& material, const mat4& transform,
        bool outline, bool setTransform = true) {
        static_assert(std::is_base_of_v<MaterialBase, MaterialT>, "Material must inherit from MaterialBase");

        MaterialSetupFunction setupFunction;
        if constexpr(opaque) {
            setupFunction = MaterialT::SetupOpaquePipeline;
        }
        else {
            setupFunction = MaterialT::SetupTransparentPipeline;
        }

        auto& dc = _drawCalls.emplace_back();
        dc.Transform = transform;
        dc.VertexBuffer = vertexBuffer;
        dc.IndexBuffer = indexBuffer;
        dc.Material = material;
        dc.Submesh = submesh;
        dc.MaterialSetup = setupFunction;
        dc.DrawType = EDrawTypeDefault;
        dc.SetTransform = setTransform;

        if (outline) {
            _outlinedObjects.push_back({
                (uint16)(_drawCalls.size() - 1)
            });
        }
    }

    template<bool opaque, class MaterialT>
    void Common(uint32 dispatchX, uint32 dispatchY, uint32 dispatchZ, const Ptr<MaterialT>& material, const mat4& transform,
        bool outline, bool setTransform = true) {
        static_assert(std::is_base_of_v<MaterialBase, MaterialT>, "Material must inherit from material_base.");

        MaterialSetupFunction setupFunction;
        if constexpr (opaque) {
            setupFunction = MaterialT::SetupOpaquePipeline;
        }
        else {
            setupFunction = MaterialT::SetupTransparentPipeline;
        }

        auto& dc = _drawCalls.emplace_back();
        dc.Transform = transform;
        dc.Material = material;
        dc.DispatchInfo = { dispatchX, dispatchY, dispatchZ };
        dc.MaterialSetup = setupFunction;
        dc.DrawType = EDrawTypeMeshShader;
        dc.SetTransform = setTransform;

        if (outline) {
            _outlinedObjects.push_back({
                (uint16)(_drawCalls.size() - 1)
            });
        }
    }

private:
    enum EDrawType {
        EDrawTypeDefault,
        EDrawTypeMeshShader
    };

    struct DispatchInfo {
        uint32 DispatchX, DispatchY, DispatchZ;
    };

    struct DrawCall {
        mat4 Transform;
        Ptr<DxVertexBuffer> VertexBuffer;
        Ptr<DxIndexBuffer> IndexBuffer;
        Ptr<MaterialBase> Material;
        union {
            SubmeshInfo Submesh;
            DispatchInfo DispatchInfo;
        };
        MaterialSetupFunction MaterialSetup;
        EDrawType DrawType;
        bool SetTransform;
    };

    std::vector<DrawCall> _drawCalls;
    std::vector<uint16> _outlinedObjects;

    friend class DxRenderer;
};

// Renders opaque objects. It also generates screen space velocities, which is why there are three methods for static, dynamic and animated objects.
class OpaqueRenderPass : public GeometryRenderPass {
public:
    template<class MaterialT>
    void RenderStaticObject(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh,
        const Ptr<MaterialT>& material, const mat4& transform, uint16 objectId, bool outline = false) {
        Common<true>(vertexBuffer, indexBuffer, submesh, material, transform, outline);

        _staticDepthOnlyDrawCalls.push_back({ transform, vertexBuffer, indexBuffer, submesh, objectId });
    }

    template <typename MaterialT>
    void RenderDynamicObject(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh,
        const Ptr<MaterialT>& material, const mat4& transform, const mat4& prevFrameTransform, uint16 objectID, bool outline = false) {
        Common<true>(vertexBuffer, indexBuffer, submesh, material, transform, outline);

        _dynamicDepthOnlyDrawCalls.push_back({transform, prevFrameTransform, vertexBuffer, indexBuffer, submesh, objectID});
    }

    template <typename MaterialT>
    void RenderAnimatedObject(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxVertexBuffer>& prevFrameVertexBuffer,
        const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh, SubmeshInfo prevFrameSubmesh, const Ptr<MaterialT>& material,
        const mat4& transform, const mat4& prevFrameTransform,
        uint16 objectID, bool outline = false) {
        Common<true>(vertexBuffer, indexBuffer, submesh, material, transform, outline);

        _animatedDepthOnlyDrawCalls.push_back(
            {
                transform, prevFrameTransform, vertexBuffer,
                prevFrameVertexBuffer ? prevFrameVertexBuffer : vertexBuffer,
                indexBuffer,
                submesh,
                prevFrameVertexBuffer ? prevFrameSubmesh : submesh,
                objectID
            }
        );
    }

    void Reset();

private:
    struct StaticDepthOnlyDrawCall {
        mat4 Transform;
        Ptr<DxVertexBuffer> VertexBuffer;
        Ptr<DxIndexBuffer> IndexBuffer;
        SubmeshInfo Submesh;
        uint16 ObjectID;
    };

    struct DynamicDepthOnlyDrawCall {
        mat4 Transform;
        mat4 PrevFrameTransform;
        Ptr<DxVertexBuffer> VertexBuffer;
        Ptr<DxIndexBuffer> IndexBuffer;
        SubmeshInfo Submesh;
        uint16 ObjectID;
    };

    struct AnimatedDepthOnlyDrawCall {
        mat4 Transform;
        mat4 PrevFrameTransform;
        Ptr<DxVertexBuffer> VertexBuffer;
        Ptr<DxVertexBuffer> PrevFrameVertexBuffer;
        Ptr<DxIndexBuffer> IndexBuffer;
        SubmeshInfo Submesh;
        SubmeshInfo PrevFrameSubmesh;
        uint16 ObjectID;
    };

    std::vector<StaticDepthOnlyDrawCall> _staticDepthOnlyDrawCalls;
    std::vector<DynamicDepthOnlyDrawCall> _dynamicDepthOnlyDrawCalls;
    std::vector<AnimatedDepthOnlyDrawCall> _animatedDepthOnlyDrawCalls;

    friend class DxRenderer;
};

// Transparent pass currently generates no screen velocities and no object ids.
struct TransparentRenderPass : public GeometryRenderPass {
    template <typename MaterialT>
    void RenderObject(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh, const Ptr<MaterialT>& material, const mat4& transform,
        bool outline = false) {
        Common<false>(vertexBuffer, indexBuffer, submesh, material, transform, outline);
    }

    friend class DxRenderer;
};

struct OverlayRenderPass : public GeometryRenderPass {
    template <typename MaterialT>
    void RenderObject(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh, const Ptr<MaterialT>& material, const mat4& transform, bool setTransform) {
        Common<true>(vertexBuffer, indexBuffer, submesh, material, transform, false, setTransform);
    }

    template <typename MaterialT>
    void RenderObjectWithMeshShader(uint32 dispatchX, uint32 dispatchY, uint32 dispatchZ, const Ptr<MaterialT>& material, const mat4& transform, bool setTransform) {
        Common<true>(dispatchX, dispatchY, dispatchZ, material, transform, false, setTransform);
    }

    friend class DxRenderer;
};

// Base for all shadow map passes.
class ShadowRenderPass {
protected:
    struct DrawCall {
        mat4 Transform;
        Ptr<DxVertexBuffer> VertexBuffer;
        Ptr<DxIndexBuffer> IndexBuffer;
        SubmeshInfo Submesh;
    };

    friend class DxRenderer;
};

class SunShadowRenderPass : public ShadowRenderPass {
public:
    vec4 Viewports[MAX_NUM_SHADOW_CASCADES];

    // Since each cascade includes the next lower one, if you submit a draw to cascade N, it will also be rendered in N-1 automatically. No need to add it to the lower one.
    void RenderObject(uint32 cascadeIndex, const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh, const mat4& transform);

    void Reset();

private:
    std::vector<DrawCall> _drawCalls[MAX_NUM_SHADOW_CASCADES];

    friend class DxRenderer;
};

class SpotShadowRenderPass : public ShadowRenderPass {
public:
    mat4 ViewProjMatrix;
    vec4 Viewport;

    void RenderObject(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh, const mat4& transform);

    void Reset();

private:
    std::vector<DrawCall> _drawCalls;

    friend class DxRenderer;
};

class PointShadowRenderPass : public ShadowRenderPass {
public:
    vec4 Viewport0;
    vec4 Viewport1;
    vec3 LightPosition;
    float MaxDistance;

    // TODO: Split this into positive and negative direction for frustum culling.
    void RenderObject(const Ptr<DxVertexBuffer>& vertexBuffer, const Ptr<DxIndexBuffer>& indexBuffer, SubmeshInfo submesh, const mat4& transform);

    void Reset();

private:
    std::vector<DrawCall> _drawCalls;

    friend class DxRenderer;
};

