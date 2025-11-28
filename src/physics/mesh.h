#pragma once

#include "bounding_volumes.h"
#include "../directx/DxBuffer.h"
#include "../animation/animation.h"
#include "geometry.h"

class PbrMaterial;

class Submesh {
public:
    SubmeshInfo Info;
    BoundingBox AABB;
    trs Transform;

    Ptr<PbrMaterial> Material;
    std::string name;
};

struct CompositeMesh {
    std::vector<Submesh> Submeshes;
    AnimationSkeleton Skeleton;
    DxMesh Mesh;
    BoundingBox AABB;

    std::string Filepath;
    uint32 Flags;
};

Ptr<CompositeMesh> LoadMeshFromFile(const char* sceneFilename, uint32 flags = EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithUvs | EMeshCreationFlagsWithNormals | EMeshCreationFlagsWithTangents);

// Same function but with different default flags (includes skin).
inline Ptr<CompositeMesh> LoadAnimatedMeshFromFile(const char* sceneFilename, uint32 flags = EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithUvs | EMeshCreationFlagsWithNormals | EMeshCreationFlagsWithTangents | EMeshCreationFlagsWithSkin) {
    return LoadMeshFromFile(sceneFilename, flags);
}