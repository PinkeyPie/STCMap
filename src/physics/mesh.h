#pragma once
#include "colliders.h"
#include "../directx/DxBuffer.h"

struct aiScene;

struct SingleMesh {
    SubmeshInfo Submesh;
    AABBCollider BoundingBox;
    std::string Name;
};

struct LodMesh {
    uint32 FirstMesh;
    uint32 NumMeshes;
};

struct CompositeMesh {
    std::vector<SingleMesh> SingleMeshes;
    std::vector<LodMesh> Lods;
    std::vector<float> LodDistances;
    DxMesh Mesh;
};

const aiScene* LoadAssimpScene(const char* filepathRaw);
void FreeScene(const aiScene* scene);
CompositeMesh CreateCompositeMeshFromScene(const aiScene* scene, uint32 flags);
CompositeMesh CreateCompositeMeshFromFile(const char* sceneFilename, uint32 flags);