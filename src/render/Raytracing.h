#pragma once

#include "../physics/mesh.h"
#include "../directx/DxBuffer.h"
#include "../core/math.h"
#include "../directx/DxPipeline.h"

// Formula for hit shader index calculation: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#hit-group-table-indexing

enum ERaytracingAsRebuildMode {
    ERaytracingAsRebuild,
    ERaytracingAsRefit
};

enum ERaytracingGeometryType {
    ERaytracingMeshGeometry,
    ERaytracingProceduralGeometry,
};

struct RaytracingBlasGeometry {
    ERaytracingGeometryType Type;

    // Only valid for mesh geometry
    Ptr<DxVertexBuffer> VertexBuffer;
    Ptr<DxIndexBuffer> IndexBuffer;
    SubmeshInfo Submesh;
};

struct RaytracingBlas {
    Ptr<DxBuffer> Scratch;
    Ptr<DxBuffer> Blas;

    std::vector<RaytracingBlasGeometry> Geometries;
};

class RaytracingBlasBuilder {
public:
    RaytracingBlasBuilder& Push(Ptr<DxVertexBuffer> vertexBuffer, Ptr<DxIndexBuffer> indexBuffer, SubmeshInfo submesh, bool opaque = true, const trs& localTransform = trs::identity);
    RaytracingBlasBuilder& Push(const std::vector<BoundingBox>& boundingBox, bool opaque);
    Ptr<RaytracingBlas> Finish(bool keepScratch = false);

private:
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> _geometryDescs;

    std::vector<RaytracingBlasGeometry> _geometries;

    std::vector<mat4> _localTransforms;             // For meshes.
    std::vector<D3D12_RAYTRACING_AABB> _aabbDescs;  // For procedurals.
};

struct RaytracingObjectType {
    Ptr<RaytracingBlas> Blas;
    uint32 InstanceContributionToHitGroupIndex;
};

struct RaytracingShader {
    void* Mesh;
    void* Procedural;
};

struct RaytracingShaderBindingTableDesc {
    uint32 EntrySize;

    void* RayGen;
    std::vector<void*> Miss;
    std::vector<RaytracingShader> HitGroups;

    uint32 RayGenOffset;
    uint32 MissOffset;
    uint32 HitOffset;
};

struct DxRaytracingPipeline {
    DxRaytracingPipelineState Pipeline;
    DxRootSignature RootSignature;

    RaytracingShaderBindingTableDesc ShaderBindingTableDesc;
};

struct RaytracingMeshHitGroup {
    const wchar* CloseHit;  // Optional
    const wchar* AnyHit;    // Optional
};

struct RaytracingProceduralHitGroup {
    const wchar* Intersection;
    const wchar* ClosestHit;    // Optional
    const wchar* AnyHit;        // Optional
};

struct RaytracingPipelineBuilder {
    RaytracingPipelineBuilder(const wchar* shaderFilename, uint32 payloadSize, uint32 maxRecursionDepth, bool hasMeshGeometry, bool ProceduralGeometry);

    RaytracingPipelineBuilder& GlobalRootSignature(D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc);
    RaytracingPipelineBuilder& RayGen(const wchar* entryPoint, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {});

    // The root signature describes parameters for both hit shaders. Miss will not get any arguments for now.
    RaytracingPipelineBuilder& HitGroup(const wchar* groupName, const wchar* miss,
        RaytracingMeshHitGroup mesh, D3D12_ROOT_SIGNATURE_DESC meshRootSignatureDesc = {},
        RaytracingProceduralHitGroup procedural = {}, D3D12_ROOT_SIGNATURE_DESC proceduralRootSignatureDesc = {});

    DxRaytracingPipeline Finish();

private:
    struct RaytracingRootSignature {
        DxRootSignature RootSignature;
        ID3D12RootSignature* RootSignaturePtr;
    };

    RaytracingRootSignature CreateRaytracingRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);

    RaytracingRootSignature _globalRS;
    RaytracingRootSignature _rayGenRS;

    std::vector<const wchar*> _emptyAssociations;
    std::vector<const wchar*> _allExports;

    const wchar* _rayGenEntryPoint;
    std::vector<const wchar*> _missEntryPoints;

    std::vector<const wchar*> _shaderNameDefines;

    uint32 _payloadSize;
    uint32 _maxRecursionDepth;

    bool _hasMeshGeometry;
    bool _hasProceduralGeometry;

    uint32 _tableEntrySize = 0;

    const wchar* _shaderFilename;

    // Since these store pointers to each other, they are not resizable arrays.
    D3D12_STATE_SUBOBJECT _subobjects[512];
    uint32 _numSubobjects = 0;

    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION _associations[16];
    uint32 _numAssociations = 0;

    D3D12_HIT_GROUP_DESC _hitGroups[8];
    uint32 _numHitGroups = 0;

    D3D12_EXPORT_DESC _exports[24];
    uint32 _numExports = 0;

    const wchar* _stringBuffer[128];
    uint32 _numStrings = 0;

    RaytracingRootSignature _rootSignatures[8];
    uint32 _numRootSignatures = 0;

    std::wstring _groupNameStorage[8];
    uint32 _groupNameStoragePtr = 0;

};