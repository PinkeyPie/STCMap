#include "mesh.h"
#include "geometry.h"
#include "../render/pbr.hpp"

#include "assimp.h"

namespace {
    void GetMeshNamesAndTransforms(const aiNode *node, Ptr<CompositeMesh> &mesh,
                                   const mat4 &parentTransform = mat4::identity) {
        mat4 transform = parentTransform * ReadAssimpMatrix(node->mTransformation);
        for (uint32 i = 0; i < node->mNumMeshes; ++i) {
            uint32 meshIndex = node->mMeshes[i];
            auto &submesh = mesh->Submeshes[meshIndex];
            submesh.name = node->mName.C_Str();
            submesh.Transform = transform;
        }

        for (uint32 i = 0; i < node->mNumChildren; ++i) {
            GetMeshNamesAndTransforms(node->mChildren[i], mesh, transform);
        }
    }
}

Ptr<CompositeMesh> LoadMeshFromFile(const char *sceneFilename, uint32 flags) {
    Assimp::Importer importer;

    const aiScene *scene = LoadAssimpSceneFile(sceneFilename, importer);

    if (!scene) {
        return 0;
    }

    CpuMesh cpuMesh(flags);

    Ptr<CompositeMesh> result = MakePtr<CompositeMesh>();

    if (flags & EMeshCreationFlagsWithSkin) {
        result->Skeleton.LoadFromAssimp(scene, 1.f);

#if 0
        result->skeleton.prettyPrintHierarchy();

        for (uint32 i = 0; i < (uint32)result->skeleton.joints.size(); ++i)
        {
            auto& joint = result->skeleton.joints[i];

            auto it = result->skeleton.nameToJointID.find(joint.name);
            assert(it != result->skeleton.nameToJointID.end());
            assert(it->second == i);
        }
#endif

        for (uint32 i = 0; i < scene->mNumAnimations; ++i) {
            result->Skeleton.PushAssimpAnimation(sceneFilename, scene->mAnimations[i], 1.f);
        }
    }

    result->Submeshes.resize(scene->mNumMeshes);
    GetMeshNamesAndTransforms(scene->mRootNode, result);

    result->AABB = BoundingBox::NegativeInfinity();

    for (uint32 m = 0; m < scene->mNumMeshes; ++m) {
        Submesh &sub = result->Submeshes[m];

        aiMesh *mesh = scene->mMeshes[m];
        sub.Info = cpuMesh.PushAssimpMesh(mesh, 1.f, &sub.AABB,
                                          (flags & EMeshCreationFlagsWithSkin) ? &result->Skeleton : 0);
        sub.Material = scene->HasMaterials()
                           ? LoadAssimpMaterial(scene->mMaterials[mesh->mMaterialIndex])
                           : GetDefaultPBRMaterial();

        result->AABB.Grow(sub.AABB.MinCorner);
        result->AABB.Grow(sub.AABB.MaxCorner);
    }

    result->Mesh = cpuMesh.CreateDxMesh();

    return result;
    result->Filepath = sceneFilename;
    result->Flags = flags;
    return result;
}
