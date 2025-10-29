#include "mesh.h"
#include "../physics/colliders.h"
#include "../physics/geometry.h"

#include "assimp/Importer.hpp"
#include "assimp/Exporter.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

#define CACHE_FORMAT "assbin" // We don't export as assbin, because the exporter kills the mesh names, which we use to identify LODs

namespace {
    void FixUpMeshNames(const aiScene* scene, const aiNode* root) {
        for (uint32 i = 0; i < root->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[root->mMeshes[i]];
            mesh->mName = root->mName;
        }

        for (uint32 i = 0; i < root->mNumChildren; i++) {
            FixUpMeshNames(scene, root->mChildren[i]);
        }
    }
}

const aiScene *LoadAssimpScene(const char *filepathRaw) {
    fs::path filepath = filepathRaw;
    fs::path extension = filepath.extension();

    fs::path cachedFilename = filepath;
    cachedFilename.replace_extension(".cache." CACHE_FORMAT);

    fs::path cacheFilepath = L"bin_cache" / cachedFilename;

    Assimp::Importer importer;
    const aiScene* scene = nullptr;

    {
        // Look for cached

        WIN32_FILE_ATTRIBUTE_DATA cachedData;
        if (GetFileAttributesExW(cacheFilepath.c_str(), GetFileExInfoStandard, &cachedData)) {
            FILETIME cachedFiletime = cachedData.ftLastWriteTime;

            WIN32_FILE_ATTRIBUTE_DATA originalData;
            assert(GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &originalData));
            FILETIME originalFiletime = originalData.ftLastWriteTime;

            if (CompareFileTime(&cachedFiletime, &originalFiletime) >= 0) {
                // Cached file is newer than original, so load this
                scene = importer.ReadFile(cacheFilepath.string(), 0);
            }
        }
    }

    if (not scene) {
        importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.f);
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
        importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, UINT16_MAX); // So that we can use 16 bit indices.

        uint32 importFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs;
        uint32 exportFlags = 0;

        scene = importer.ReadFile(filepath.string(), importFlags);

        fs::create_directories(cacheFilepath.parent_path());
        Assimp::Exporter exporter;

#if 0
        uint32 exporterCount = exporter.GetExportFormatCount();
        for (uint32 i = 0; i < exporterCount; i++) {
            const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
        }
#endif
        exporter.Export(scene, CACHE_FORMAT, cacheFilepath.string(), exportFlags);
    }

    if (scene) {
        scene = importer.GetOrphanedScene();
        FixUpMeshNames(scene, scene->mRootNode);
    }

    return scene;
}

void FreeScene(const aiScene *scene) {
    if (scene) {
        scene->~aiScene(); // Todo: I'm not sure that's deletes everything
    }
}

CompositeMesh CreateCompositeMeshFromFile(const char *sceneFilename, uint32 flags) {
    const aiScene* scene = LoadAssimpScene(sceneFilename);
    auto mesh = CreateCompositeMeshFromScene(scene, flags);
    FreeScene(scene);
    return mesh;
}

CompositeMesh CreateCompositeMeshFromScene(const aiScene *scene, uint32 flags) {
    struct MeshInfo {
        int32 Lod;
        SubmeshInfo Submesh;
        AABBCollider Aabb;
        std::string Name;
    };

    CpuMesh cpuMesh(flags);

    std::vector<MeshInfo> infos(scene->mNumMeshes);

    int32 maxLOD = 0;
    uint32 numMeshesWithoutLOD = 0;

    for (uint32 i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        std::string name = mesh->mName.C_Str();

        int32 lod = -1;

        if (name.length() >= 4) {
            uint32 j;
            for (j = (uint32)name.length() - 1; j >= 3; j--) {
                if (name[j] < '0' or name[j] > '9') {
                    break;
                }
            }
            if (j < (uint32)name.length() - 1) {
                if (    (name[j - 2] == 'L' or name[j - 2] == 'l')
                    and (name[j - 1] == 'O' or name[j - 1] == 'o')
                    and (name[j]     == 'D' or name[j]     == 'd')) {
                    lod = atoi(name.c_str() + j + 1);
                }
            }
        }

        maxLOD = max(maxLOD, lod);

        if (lod == -1) {
            ++numMeshesWithoutLOD;
        }

        infos[i].Lod = lod;
        infos[i].Submesh = cpuMesh.PushAssimpMesh(mesh, 1.f, &infos[i].Aabb);
        infos[i].Name = name;
    }

    std::sort(infos.begin(), infos.end(), [](const MeshInfo& a, const MeshInfo& b) {
        return a.Lod < b.Lod;
    });

    CompositeMesh result;
    result.Lods.resize(maxLOD + 1);

    uint32 currentInfoOffset = numMeshesWithoutLOD;
    int32 lodReduce = 0;

    for (int32 i = 0; i <= maxLOD; i++) {
        LodMesh& lod = result.Lods[i];
        lod.FirstMesh = (uint32)result.SingleMeshes.size();
        lod.NumMeshes = 0;

        if (currentInfoOffset < infos.size()) {
            while (infos[currentInfoOffset].Lod - lodReduce != i) {
                ++lodReduce;
            }
        }

        for (uint32 j = 0; j < numMeshesWithoutLOD; j++, lod.NumMeshes++) {
            result.SingleMeshes.push_back({infos[j].Submesh, infos[j].Aabb, infos[j].Name});
        }

        if (currentInfoOffset < infos.size()) {
            while (currentInfoOffset < infos.size() and infos[currentInfoOffset].Lod - lodReduce == i) {
                result.SingleMeshes.push_back({infos[currentInfoOffset].Submesh, infos[currentInfoOffset].Aabb, infos[currentInfoOffset].Name});
                ++currentInfoOffset;
                ++lod.NumMeshes;
            }
        }
    }

    result.Lods.resize(result.Lods.size() - lodReduce);

    if (lodReduce > 0) {
        std::cout << "There is a gap in the LOD declarations." << std::endl;
    }

    result.LodDistances.resize(result.Lods.size());

    std::cout << "There are " << result.Lods.size() << " LODs in this file" << std::endl;

    uint32 l = 0;
    for (LodMesh& lod : result.Lods) {
        std::cout << "LOD " << l++ << ": " << std::endl;
        for (uint32 i = lod.FirstMesh; i < lod.FirstMesh + lod.NumMeshes; i++) {
            std::cout << "   "
            << result.SingleMeshes[i].Name << " -- "
            << result.SingleMeshes[i].Submesh.NumVertices << " vertices, "
            << result.SingleMeshes[i].Submesh.NumTriangles << " triangles. " << std::endl;
        }
    }

    for (uint32 i = 0; i < (uint32)result.LodDistances.size(); i++) {
        result.LodDistances[i] = float(i); // Todo: better default values
    }

    result.Mesh = cpuMesh.CreateDxMesh();

    return result;
}
