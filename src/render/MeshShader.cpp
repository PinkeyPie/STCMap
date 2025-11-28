#include "../directx/DxPipeline.h"
#include "../directx/DxRenderer.h"
#include "MeshShader.h"

#include <iostream>

static DxPipeline cubePipeline;
static DxPipeline meshPipeline;
static DxPipeline blobPipeline;

static Ptr<DxBuffer> marchingCubesBuffer;

struct MarchingCubesLookup {
    uint32 Indices[4];
    uint8 Vertices[12];
    uint16 TriangleCount;
    uint16 VertexCount;
};

extern const MarchingCubesLookup marchingCubesLookup[256];

struct Subset {
    uint32 Offset;
    uint32 Count;
};

struct MeshShaderSubmeshInfo {
    uint32 FirstVertex;
    uint32 NumVertices;

    uint32 FirstMeshlet;
    uint32 NumMeshlets;

    uint32 FirstUniqueVertexIndex;
    uint32 NumUniqueVertexIndices;

    uint32 FirstPackedTriangle;
    uint32 NumPackedTriangles;

    uint32 FirstMeshletSubset;
    uint32 NumMeshletSubsets;
};

struct MeshShaderMesh {
    std::vector<MeshShaderSubmeshInfo> Submeshes;
    std::vector<Subset> Subsets;

    Ptr<DxBuffer> Vertices;
    Ptr<DxBuffer> Meshlets;
    Ptr<DxBuffer> UniqueVertexIndices;
    Ptr<DxBuffer> PrimitiveIndices;
};

static Ptr<MeshShaderMesh> LoadMeshShaderMeshFromFile(const char *filename);


struct MeshShaderCubeMaterial : MaterialBase {
    static void SetupOpaquePipeline(DxCommandList *cl, const CommonMaterialInfo &info) {
        cl->SetPipelineState(*cubePipeline.Pipeline);
        cl->SetGraphicsRootSignature(*cubePipeline.RootSignature);
    }

    void PrepareForRendering(DxCommandList *cl) {
    }
};

struct MeshShaderMeshMaterial : MaterialBase {
    Ptr<MeshShaderMesh> Mesh;

    static void SetupOpaquePipeline(DxCommandList *cl, const CommonMaterialInfo &info) {
        cl->SetPipelineState(*meshPipeline.Pipeline);
        cl->SetGraphicsRootSignature(*meshPipeline.RootSignature);
    }

    void PrepareForRendering(DxCommandList *cl) {
        cl->SetRootGraphicsSRV(1, Mesh->Vertices);
        cl->SetRootGraphicsSRV(2, Mesh->Meshlets);
        cl->SetRootGraphicsSRV(3, Mesh->UniqueVertexIndices);
        cl->SetRootGraphicsSRV(4, Mesh->PrimitiveIndices);
    }
};

struct MeshShaderBlobMaterial : MaterialBase {
    static void SetupOpaquePipeline(DxCommandList *cl, const CommonMaterialInfo &info) {
        cl->SetPipelineState(*blobPipeline.Pipeline);
        cl->SetGraphicsRootSignature(*blobPipeline.RootSignature);

        cl->SetGraphicsDynamicConstantBuffer(1, info.CameraCBV);
        cl->SetRootGraphicsSRV(2, marchingCubesBuffer);

        cl->SetDescriptorHeapSRV(3, 0, info.Sky);
    }

    struct MetaBall {
        vec3 Pos;
        vec3 Dir;
        float Radius;
    };

    static const uint32 DEFAULT_SHIFT = 7; // 128x128x128 cubes by default. (1 << 7) == 128
    static const uint32 DEFAULT_BALL_COUNT = 32;
    static const uint32 MAX_BALL_COUNT = 128;
    static const uint32 BALL_COUNT = DEFAULT_BALL_COUNT;
    static const uint32 SHIFT = DEFAULT_SHIFT;

    MetaBall Balls[MAX_BALL_COUNT];

    struct ConstantCb {
        vec4 Balls[MAX_BALL_COUNT];
    };

    MeshShaderBlobMaterial() {
        for (uint32 i = 0; i < BALL_COUNT; ++i) {
            // Random positions in [0.25, 0.75]
            Balls[i].Pos.x = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.5f + 0.25f;
            Balls[i].Pos.y = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.5f + 0.25f;
            Balls[i].Pos.z = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.5f + 0.25f;

            // Random directions in [-0.6, 0.6]
            Balls[i].Dir.x = (float(rand() % RAND_MAX) / float(RAND_MAX - 1) - 0.5f) * 1.2f;
            Balls[i].Dir.y = (float(rand() % RAND_MAX) / float(RAND_MAX - 1) - 0.5f) * 1.2f;
            Balls[i].Dir.z = (float(rand() % RAND_MAX) / float(RAND_MAX - 1) - 0.5f) * 1.2f;

            // Random radius in [0.02, 0.06]
            Balls[i].Radius = float(rand() % RAND_MAX) / float(RAND_MAX - 1) * 0.04f + 0.02f;
        }
    }

    void PrepareForRendering(DxCommandList *cl) {
        ConstantCb cb;

        float animation_speed = 0.5f;
        animation_speed *= animation_speed;
        const float frame_time = animation_speed * 0.001f;
        for (uint32 i = 0; i < BALL_COUNT; ++i) {
            vec3 d = vec3(0.5f, 0.5f, 0.5f) - Balls[i].Pos;
            Balls[i].Dir += d * (5.0f * frame_time / (2.0f + dot(d, d)));
            Balls[i].Pos += Balls[i].Dir * frame_time;

            float radius = Balls[i].Radius;
            cb.Balls[i] = vec4(Balls[i].Pos, radius * radius);
        }

        auto b = DxContext::Instance().UploadDynamicConstantBuffer(cb);

        cl->SetGraphicsDynamicConstantBuffer(0, b);
    }
};

static Ptr<MeshShaderCubeMaterial> cubeMaterial;
static Ptr<MeshShaderMeshMaterial> meshMaterial;
static Ptr<MeshShaderBlobMaterial> blobMaterial;

void InitializeMeshShader() {
    D3D12_RT_FORMAT_ARRAY renderTargetFormat = {};
    DxPipelineFactory* pipelineFactory = DxPipelineFactory::Instance();
    renderTargetFormat.NumRenderTargets = 1;
    renderTargetFormat.RTFormats[0] = DxRenderer::overlayFormat; {
        struct PipelineStateStream : DxPipelineStreamBase {
            // Will be set by reloader.
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_MS ms;
            CD3DX12_PIPELINE_STATE_STREAM_PS ps;

            // Initialized here.
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;

            void SetRootSignature(DxRootSignature rs) override { rootSignature = rs.RootSignature(); }
            void SetMeshShader(DxBlob blob) override { ms = CD3DX12_SHADER_BYTECODE(blob.Get()); }
            void SetPixelShader(DxBlob blob) override { ps = CD3DX12_SHADER_BYTECODE(blob.Get()); }
        };

        PipelineStateStream stream;
        stream.dsvFormat = DxRenderer::overlayDepthFormat;
        stream.rtvFormats = renderTargetFormat;

        GraphicsPipelineFiles files = {};
        files.MS = "mesh_shader_v0_ms";
        files.PS = "mesh_shader_ps";

        cubePipeline = pipelineFactory->CreateReloadablePipelineFromStream(stream, files, ERsInMeshShader);

        files.MS = "mesh_shader_v2_ms";
        meshPipeline = pipelineFactory->CreateReloadablePipelineFromStream(stream, files, ERsInMeshShader);

        cubeMaterial = MakePtr<MeshShaderCubeMaterial>();
        meshMaterial = MakePtr<MeshShaderMeshMaterial>();
        meshMaterial->Mesh = LoadMeshShaderMeshFromFile("assets/meshes/Dragon_LOD0.bin");
    } {
        struct PipelineStateStream : DxPipelineStreamBase {
            // Will be set by reloader.
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE rootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_AS as;
            CD3DX12_PIPELINE_STATE_STREAM_MS ms;
            CD3DX12_PIPELINE_STATE_STREAM_PS ps;

            // Initialized here.
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT dsvFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
            CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER rasterizer;

            void SetRootSignature(DxRootSignature rs) override { rootSignature = rs.RootSignature(); }
            void SetAmplificationShader(DxBlob blob) override { as = CD3DX12_SHADER_BYTECODE(blob.Get()); }
            void SetMeshShader(DxBlob blob) override { ms = CD3DX12_SHADER_BYTECODE(blob.Get()); }
            void SetPixelShader(DxBlob blob) override { ps = CD3DX12_SHADER_BYTECODE(blob.Get()); }
        };

        auto rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        //rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rasterizerDesc.FrontCounterClockwise = TRUE; // Righthanded coordinate system.

        PipelineStateStream stream;
        stream.dsvFormat = DxRenderer::overlayDepthFormat;
        stream.rtvFormats = renderTargetFormat;
        stream.rasterizer = rasterizerDesc;

        GraphicsPipelineFiles files = {};
        files.AS = "mesh_shader_v4_as";
        files.MS = "mesh_shader_v4_ms";
        files.PS = "mesh_shader_v4_ps";

        marchingCubesBuffer = DxBuffer::Create(sizeof(MarchingCubesLookup), arraysize(marchingCubesLookup),
                                               (void *) marchingCubesLookup);

        blobPipeline = pipelineFactory->CreateReloadablePipelineFromStream(stream, files, ERsInMeshShader);

        blobMaterial = MakePtr<MeshShaderBlobMaterial>();
    }
}

void TestRenderMeshShader(OverlayRenderPass *overlayRenderPass) {
    /*overlayRenderPass->renderObjectWithMeshShader(1, 1, 1,
        cubeMaterial,
        createModelMatrix(vec3(0.f, 30.f, 0.f), quat::identity, 1.f),
        true
    );*/


    /*if (meshMaterial->mesh)
    {
        auto& sm = meshMaterial->mesh->submeshes[0];
        overlayRenderPass->renderObjectWithMeshShader(sm.numMeshlets, 1, 1,
            meshMaterial,
            createModelMatrix(vec3(0.f, 30.f, 0.f), quat::identity, 0.3f),
            true
        );
    }*/

    const uint32 gridSize = (1 << blobMaterial->SHIFT);

    overlayRenderPass->RenderObjectWithMeshShader((gridSize / 4) * (gridSize / 4) * (gridSize / 4), 1, 1,
                                                  blobMaterial,
                                                  mat4::identity,
                                                  false);
}

#include <fstream>

enum EAttributeType {
    EAttributeTypePosition,
    EAttributeTypeNormal,
    EAttributeTypeTexCoord,
    EAttributeTypeTangent,
    EAttributeTypeBitangent,
    EAttributeTypeCount,
};

struct MeshHeader {
    uint32 Indices;
    uint32 IndexSubsets;
    uint32 Attributes[EAttributeTypeCount];

    uint32 Meshlets;
    uint32 MeshletSubsets;
    uint32 UniqueVertexIndices;
    uint32 PrimitiveIndices;
    uint32 CullData;
};

struct BufferView {
    uint32 Offset;
    uint32 Size;
};

struct BufferAccessor {
    uint32 BufferView;
    uint32 Offset;
    uint32 Size;
    uint32 Stride;
    uint32 Count;
};

struct FileHeader {
    uint32 Prolog;
    uint32 Version;

    uint32 MeshCount;
    uint32 AccessorCount;
    uint32 BufferViewCount;
    uint32 BufferSize;
};

enum EFileVersion {
    EFileVersionInitial = 0,
    ECurrentFileVersion = EFileVersionInitial
};

// Meshlet stuff.

struct MeshletInfo {
    uint32 NumVertices;
    uint32 FirstVertex;
    uint32 NumPrimitives;
    uint32 FirstPrimitive;
};

struct PackedTriangle {
    uint32 I0: 10;
    uint32 I1: 10;
    uint32 I2: 10;
};

struct MeshVertex {
    vec3 Position;
    vec3 Normal;
};

static Ptr<MeshShaderMesh> LoadMeshShaderMeshFromFile(const char *filename) {
    std::ifstream stream(filename, std::ios::binary);
    if (!stream.is_open()) {
        std::cerr << "Could not find file '" << filename << "'." << std::endl;
        return 0;
    }

    std::vector<MeshHeader> meshHeaders;
    std::vector<BufferView> bufferViews;
    std::vector<BufferAccessor> accessors;

    FileHeader header;
    stream.read((char *) &header, sizeof(header));

    const uint32 prolog = 'MSHL';
    if (header.Prolog != prolog) {
        return 0; // Incorrect file format.
    }

    if (header.Version != ECurrentFileVersion) {
        return 0; // Version mismatch between export and import serialization code.
    }

    // Read mesh metdata.
    meshHeaders.resize(header.MeshCount);
    stream.read((char *) meshHeaders.data(), meshHeaders.size() * sizeof(meshHeaders[0]));

    accessors.resize(header.AccessorCount);
    stream.read((char *) accessors.data(), accessors.size() * sizeof(accessors[0]));

    bufferViews.resize(header.BufferViewCount);
    stream.read((char *) bufferViews.data(), bufferViews.size() * sizeof(bufferViews[0]));

    std::vector<uint8> m_buffer;
    m_buffer.resize(header.BufferSize);
    stream.read((char *) m_buffer.data(), header.BufferSize);

    char eofbyte;
    stream.read(&eofbyte, 1); // Read last byte to hit the eof bit.
    assert(stream.eof()); // There's a problem if we didn't completely consume the file contents..

    stream.close();

    std::vector<MeshShaderSubmeshInfo> submeshes(meshHeaders.size());

    //std::vector<uint32> indices;
    //std::vector<subset> indexSubsets;
    std::vector<MeshVertex> vertices;
    std::vector<MeshletInfo> meshlets;
    std::vector<uint32> uniqueVertexIndices;
    std::vector<PackedTriangle> primitiveIndices;
    std::vector<Subset> meshletSubsets;

    for (uint32_t i = 0; i < (uint32) meshHeaders.size(); ++i) {
        MeshHeader &meshView = meshHeaders[i];
        MeshShaderSubmeshInfo &sm = submeshes[i];

#if 0
		// Index data.
		{
			buffer_accessor& accessor = accessors[meshView.indices];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			assert(accessor.size == sizeof(uint32));
			assert(accessor.count * accessor.size == bufferView.size);

			uint32* start = (uint32*)(m_buffer.data() + bufferView.offset);

			sm.firstIndex = (uint32)indices.size();
			sm.numIndices = accessor.count;

			indices.insert(indices.end(), start, start + sm.numIndices);
		}
#endif

#if 0
		// Index Subset data.
		{
			buffer_accessor& accessor = accessors[meshView.indexSubsets];
			buffer_view& bufferView = bufferViews[accessor.bufferView];

			assert(accessor.count * accessor.size == bufferView.size);

			subset* start = (subset*)(m_buffer.data() + bufferView.offset);

			sm.firstIndexSubset = (uint32)indexSubsets.size();
			sm.numIndexSubsets = accessor.count;

			indexSubsets.insert(indexSubsets.end(), start, start + sm.numIndexSubsets);
		}
#endif

        // Vertex data & layout metadata

        bool first = true;

        for (uint32 j = 0; j < EAttributeTypeCount; ++j) {
            if (meshView.Attributes[j] == -1) {
                continue;
            }

            BufferAccessor &accessor = accessors[meshView.Attributes[j]];

            BufferView &bufferView = bufferViews[accessor.BufferView];

            if (first) {
                sm.FirstVertex = (uint32) vertices.size();
                sm.NumVertices = accessor.Count;
                vertices.resize(vertices.size() + sm.NumVertices);
                first = false;
            } else {
                assert(sm.NumVertices == accessor.Count);
            }

            uint8 *data = m_buffer.data() + bufferView.Offset + accessor.Offset;

            for (uint32 vertexID = 0; vertexID < sm.NumVertices; ++vertexID) {
                MeshVertex &v = vertices[sm.FirstVertex + vertexID];

                uint8 *attributeData = data + accessor.Stride * vertexID;

                if (j == EAttributeTypePosition) {
                    v.Position = *(vec3 *) attributeData;
                } else if (j == EAttributeTypeNormal) {
                    v.Normal = *(vec3 *) attributeData;
                }
            }
        }

        // Meshlet data
        {
            BufferAccessor &accessor = accessors[meshView.Meshlets];
            BufferView &bufferView = bufferViews[accessor.BufferView];

            MeshletInfo *start = (MeshletInfo *) (m_buffer.data() + bufferView.Offset);

            assert(accessor.Count * accessor.Size == bufferView.Size);

            sm.FirstMeshlet = (uint32) meshlets.size();
            sm.NumMeshlets = accessor.Count;

            meshlets.insert(meshlets.end(), start, start + sm.NumMeshlets);
        }

        // Meshlet Subset data
        {
            BufferAccessor &accessor = accessors[meshView.MeshletSubsets];
            BufferView &bufferView = bufferViews[accessor.BufferView];

            Subset *start = (Subset *) (m_buffer.data() + bufferView.Offset);

            assert(accessor.Count * accessor.Size == bufferView.Size);

            sm.FirstMeshletSubset = (uint32) meshletSubsets.size();
            sm.NumMeshletSubsets = accessor.Count;

            meshletSubsets.insert(meshletSubsets.end(), start, start + sm.NumMeshletSubsets);
        }

        // Unique Vertex Index data
        {
            BufferAccessor &accessor = accessors[meshView.UniqueVertexIndices];
            BufferView &bufferView = bufferViews[accessor.BufferView];

            assert(accessor.Count * accessor.Size == bufferView.Size);

            sm.FirstUniqueVertexIndex = (uint32) uniqueVertexIndices.size();
            sm.NumUniqueVertexIndices = accessor.Count;

            if (accessor.Size == sizeof(uint32)) {
                uint32 *start = (uint32 *) (m_buffer.data() + bufferView.Offset);
                uniqueVertexIndices.insert(uniqueVertexIndices.end(), start, start + sm.NumUniqueVertexIndices);
            } else {
                assert(accessor.Size == sizeof(uint16));

                uint16 *start = (uint16 *) (m_buffer.data() + bufferView.Offset);

                std::vector<uint16> temp;
                temp.insert(temp.end(), start, start + sm.NumUniqueVertexIndices);

                for (uint16 t: temp) {
                    uniqueVertexIndices.push_back((uint32) t);
                }
            }
        }

        // Primitive Index data
        {
            BufferAccessor &accessor = accessors[meshView.PrimitiveIndices];
            BufferView &bufferView = bufferViews[accessor.BufferView];

            PackedTriangle *start = (PackedTriangle *) (m_buffer.data() + bufferView.Offset);

            assert(accessor.Count * accessor.Size == bufferView.Size);

            sm.FirstPackedTriangle = (uint32) primitiveIndices.size();
            sm.NumPackedTriangles = accessor.Count;

            primitiveIndices.insert(primitiveIndices.end(), start, start + sm.NumPackedTriangles);
        }

#if 0
		// Cull data
		{
			buffer_accessor& accessor = accessors[meshView.CullData];
			buffer_view& bufferView = bufferViews[accessor.BufferView];

			mesh.CullingData = MakeSpan(reinterpret_cast<CullData*>(m_buffer.data() + bufferView.Offset), accessor.Count);
		}
#endif
    }

#if 0
	// Build bounding spheres for each mesh
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_meshes.size()); ++i)
	{
		auto& m = m_meshes[i];

		uint32_t vbIndexPos = 0;

		// Find the index of the vertex buffer of the position attribute
		for (uint32_t j = 1; j < m.LayoutDesc.NumElements; ++j)
		{
			auto& desc = m.LayoutElems[j];
			if (strcmp(desc.SemanticName, "POSITION") == 0)
			{
				vbIndexPos = j;
				break;
			}
		}

		// Find the byte offset of the position attribute with its vertex buffer
		uint32_t positionOffset = 0;

		for (uint32_t j = 0; j < m.LayoutDesc.NumElements; ++j)
		{
			auto& desc = m.LayoutElems[j];
			if (strcmp(desc.SemanticName, "POSITION") == 0)
			{
				break;
			}

			if (desc.InputSlot == vbIndexPos)
			{
				positionOffset += GetFormatSize(m.LayoutElems[j].Format);
			}
		}

		XMFLOAT3* v0 = reinterpret_cast<XMFLOAT3*>(m.Vertices[vbIndexPos].data() + positionOffset);
		uint32_t stride = m.VertexStrides[vbIndexPos];

		BoundingSphere::CreateFromPoints(m.BoundingSphere, m.VertexCount, v0, stride);

		if (i == 0)
		{
			m_boundingSphere = m.BoundingSphere;
		}
		else
		{
			BoundingSphere::CreateMerged(m_boundingSphere, m_boundingSphere, m.BoundingSphere);
		}
	}
#endif

    Ptr<MeshShaderMesh> result = MakePtr<MeshShaderMesh>();
    result->Submeshes = std::move(submeshes);
    result->Subsets = std::move(meshletSubsets);
    result->Vertices = DxBuffer::Create(sizeof(MeshVertex), (uint32) vertices.size(), vertices.data());
    result->Meshlets = DxBuffer::Create(sizeof(MeshletInfo), (uint32) meshlets.size(), meshlets.data());
    result->UniqueVertexIndices = DxBuffer::Create(sizeof(uint32), (uint32) uniqueVertexIndices.size(),
                                                   uniqueVertexIndices.data());
    result->PrimitiveIndices = DxBuffer::Create(sizeof(PackedTriangle), (uint32) primitiveIndices.size(),
                                                primitiveIndices.data());

    return result;
}

const MarchingCubesLookup marchingCubesLookup[256] = {
    /*  Packed 8bit indices                                 Two 3bit (octal) corner indices defining the edge                 */
    {
        {0x00000000, 0x00000000, 0x00000000, 0x00000000}, {000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        0, 0
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {010, 020, 040, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {010, 051, 031, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {031, 020, 040, 051, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {020, 032, 062, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {010, 032, 062, 040, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {031, 010, 051, 032, 062, 020, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {031, 032, 062, 051, 040, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {031, 073, 032, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {010, 020, 040, 031, 073, 032, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {051, 073, 032, 010, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {032, 020, 040, 073, 051, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {020, 031, 073, 062, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {010, 031, 073, 040, 062, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {020, 010, 051, 062, 073, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x01020100, 0x00000203, 0x00000000, 0x00000000}, {051, 073, 040, 062, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {054, 040, 064, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {054, 010, 020, 064, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {010, 051, 031, 040, 064, 054, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {054, 051, 031, 064, 020, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {040, 064, 054, 020, 032, 062, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {062, 064, 054, 032, 010, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {051, 031, 010, 040, 064, 054, 032, 062, 020, 000, 000, 000},
        3, 9
    },
    {
        {0x03020100, 0x04030001, 0x04050301, 0x00000000}, {054, 062, 064, 051, 032, 031, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {031, 073, 032, 040, 064, 054, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {020, 064, 054, 010, 031, 073, 032, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {051, 073, 032, 010, 040, 064, 054, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x04000103, 0x01050303, 0x00000000}, {032, 051, 073, 064, 020, 054, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {020, 031, 073, 062, 064, 054, 040, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x03000302, 0x02030504, 0x00000000}, {031, 073, 062, 054, 010, 064, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {054, 040, 064, 051, 062, 010, 073, 020, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {054, 062, 064, 051, 073, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {051, 054, 075, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {051, 054, 075, 010, 020, 040, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {010, 054, 075, 031, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {040, 054, 075, 020, 031, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {051, 054, 075, 032, 062, 020, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {010, 032, 062, 040, 054, 075, 051, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {010, 054, 075, 031, 032, 062, 020, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x04000103, 0x03010503, 0x00000000}, {032, 075, 031, 040, 062, 054, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {031, 073, 032, 051, 054, 075, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {020, 040, 010, 031, 073, 032, 054, 075, 051, 000, 000, 000},
        3, 9
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {075, 073, 032, 054, 010, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x04030001, 0x04050301, 0x00000000}, {032, 075, 073, 020, 054, 040, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {073, 062, 020, 031, 051, 054, 075, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {054, 075, 051, 010, 031, 040, 073, 062, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x04000103, 0x01050303, 0x00000000}, {075, 010, 054, 062, 073, 020, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {075, 040, 054, 073, 062, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {051, 040, 064, 075, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {051, 010, 020, 075, 064, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {010, 040, 064, 031, 075, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x01020100, 0x00000203, 0x00000000, 0x00000000}, {031, 020, 075, 064, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {064, 075, 051, 040, 020, 032, 062, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x04000103, 0x01050303, 0x00000000}, {051, 064, 075, 032, 010, 062, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {032, 062, 020, 010, 040, 031, 064, 075, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {062, 031, 032, 064, 075, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {051, 040, 064, 075, 073, 032, 031, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {073, 032, 031, 051, 010, 075, 020, 064, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x04000103, 0x03010503, 0x00000000}, {040, 032, 010, 075, 064, 073, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {032, 075, 073, 020, 064, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x01020100, 0x05040203, 0x05070406, 0x00000000}, {051, 040, 075, 064, 073, 020, 031, 062, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {075, 010, 064, 051, 062, 031, 073, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {062, 010, 073, 020, 075, 040, 064, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {062, 075, 073, 064, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {064, 062, 076, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {020, 040, 010, 062, 076, 064, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {010, 051, 031, 062, 076, 064, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {040, 051, 031, 020, 062, 076, 064, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {064, 020, 032, 076, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {064, 040, 010, 076, 032, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {032, 076, 064, 020, 010, 051, 031, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x03000302, 0x05020304, 0x00000000}, {031, 032, 076, 040, 051, 064, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {073, 032, 031, 076, 064, 062, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {031, 073, 032, 020, 040, 010, 076, 064, 062, 000, 000, 000},
        3, 9
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {032, 010, 051, 073, 076, 064, 062, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {076, 064, 062, 032, 020, 073, 040, 051, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {073, 076, 064, 031, 020, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x02030200, 0x05040304, 0x00000000}, {073, 076, 064, 031, 040, 010, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04000103, 0x03010503, 0x00000000}, {010, 064, 020, 073, 051, 076, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {064, 073, 076, 040, 051, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {076, 054, 040, 062, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {020, 062, 076, 010, 054, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {040, 062, 076, 054, 051, 031, 010, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x04000103, 0x03010503, 0x00000000}, {051, 076, 054, 020, 031, 062, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {040, 020, 032, 054, 076, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x02020100, 0x00000301, 0x00000000, 0x00000000}, {010, 032, 054, 076, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {031, 010, 051, 032, 054, 020, 076, 040, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {031, 054, 051, 032, 076, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {076, 054, 040, 062, 032, 031, 073, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {031, 073, 032, 020, 062, 010, 076, 054, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x05040302, 0x07050606, 0x00000000}, {054, 040, 062, 076, 010, 051, 032, 073, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {073, 020, 051, 032, 054, 062, 076, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x03000302, 0x05020304, 0x00000000}, {040, 020, 031, 076, 054, 073, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {073, 010, 031, 076, 054, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {054, 020, 076, 040, 073, 010, 051, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {073, 054, 051, 076, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {054, 075, 051, 064, 062, 076, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {010, 020, 040, 054, 075, 051, 062, 076, 064, 000, 000, 000},
        3, 9
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {075, 031, 010, 054, 064, 062, 076, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {062, 076, 064, 040, 054, 020, 075, 031, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {064, 020, 032, 076, 075, 051, 054, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {051, 054, 075, 010, 076, 040, 032, 064, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x05040302, 0x07050606, 0x00000000}, {020, 032, 076, 064, 031, 010, 075, 054, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {076, 040, 032, 064, 031, 054, 075, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {051, 054, 075, 073, 032, 031, 064, 062, 076, 000, 000, 000},
        3, 9
    },
    {
        {0x03020100, 0x07060504, 0x0B0A0908, 0x00000000}, {076, 064, 062, 031, 073, 032, 010, 020, 040, 054, 075, 051},
        4, 12
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {064, 062, 076, 075, 073, 054, 032, 010, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {020, 040, 054, 075, 032, 073, 062, 076, 064, 000, 000, 000},
        5, 9
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {051, 054, 075, 073, 076, 031, 064, 020, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {031, 073, 076, 064, 010, 040, 051, 054, 075, 000, 000, 000},
        5, 9
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {054, 073, 010, 075, 020, 076, 064, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {064, 073, 076, 040, 075, 054, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {076, 075, 051, 062, 040, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x02030200, 0x05040304, 0x00000000}, {020, 062, 076, 010, 075, 051, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x03000302, 0x05020304, 0x00000000}, {010, 040, 062, 075, 031, 076, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {076, 020, 062, 075, 031, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x03000302, 0x02030504, 0x00000000}, {075, 051, 040, 032, 076, 020, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {051, 076, 075, 010, 032, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {031, 040, 075, 010, 076, 020, 032, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {031, 076, 075, 032, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {031, 073, 032, 051, 062, 075, 040, 076, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {010, 020, 062, 076, 051, 075, 031, 073, 032, 000, 000, 000},
        5, 9
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {062, 075, 040, 076, 010, 073, 032, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {076, 020, 062, 075, 032, 073, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {031, 076, 020, 073, 040, 075, 051, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {073, 010, 031, 076, 051, 075, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {010, 040, 020, 075, 073, 076, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {073, 076, 075, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {073, 075, 076, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {010, 020, 040, 075, 076, 073, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {051, 031, 010, 075, 076, 073, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {031, 020, 040, 051, 075, 076, 073, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {032, 062, 020, 073, 075, 076, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {062, 040, 010, 032, 073, 075, 076, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {010, 051, 031, 032, 062, 020, 075, 076, 073, 000, 000, 000},
        3, 9
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {075, 076, 073, 031, 032, 051, 062, 040, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {031, 075, 076, 032, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {031, 075, 076, 032, 020, 040, 010, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {051, 075, 076, 010, 032, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x04000103, 0x03010503, 0x00000000}, {075, 040, 051, 032, 076, 020, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {076, 062, 020, 075, 031, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x04000103, 0x01050303, 0x00000000}, {010, 062, 040, 075, 031, 076, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x04030001, 0x04050301, 0x00000000}, {020, 076, 062, 010, 075, 051, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {076, 051, 075, 062, 040, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {075, 076, 073, 054, 040, 064, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {054, 010, 020, 064, 076, 073, 075, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {031, 010, 051, 075, 076, 073, 040, 064, 054, 000, 000, 000},
        3, 9
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {073, 075, 076, 031, 064, 051, 020, 054, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x03020100, 0x07060504, 0x00000008, 0x00000000}, {020, 032, 062, 064, 054, 040, 073, 075, 076, 000, 000, 000},
        3, 9
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {075, 076, 073, 054, 032, 064, 010, 062, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x03020100, 0x07060504, 0x0B0A0908, 0x00000000}, {010, 051, 031, 054, 040, 064, 032, 062, 020, 075, 076, 073},
        4, 12
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {051, 031, 032, 062, 054, 064, 075, 076, 073, 000, 000, 000},
        5, 9
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {076, 032, 031, 075, 054, 040, 064, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x01020100, 0x05040203, 0x05070406, 0x00000000}, {031, 075, 032, 076, 020, 054, 010, 064, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {040, 064, 054, 051, 075, 010, 076, 032, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {064, 051, 020, 054, 032, 075, 076, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {040, 064, 054, 020, 075, 062, 031, 076, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {075, 062, 031, 076, 010, 064, 054, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {010, 051, 075, 076, 020, 062, 040, 064, 054, 000, 000, 000},
        5, 9
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {076, 051, 075, 062, 054, 064, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {073, 051, 054, 076, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {054, 076, 073, 051, 010, 020, 040, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {073, 031, 010, 076, 054, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x04000103, 0x01050303, 0x00000000}, {040, 031, 020, 076, 054, 073, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {073, 051, 054, 076, 062, 020, 032, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x01020100, 0x05040203, 0x05070406, 0x00000000}, {010, 032, 040, 062, 054, 073, 051, 076, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {020, 032, 062, 010, 076, 031, 054, 073, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {076, 031, 054, 073, 040, 032, 062, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {031, 051, 054, 032, 076, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {020, 040, 010, 031, 051, 032, 054, 076, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x01020100, 0x00000203, 0x00000000, 0x00000000}, {010, 054, 032, 076, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {040, 032, 020, 054, 076, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x03000302, 0x02030504, 0x00000000}, {051, 054, 076, 020, 031, 062, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {040, 031, 062, 010, 076, 051, 054, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {020, 076, 062, 010, 054, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {076, 040, 054, 062, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {064, 076, 073, 040, 051, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x03000302, 0x02030504, 0x00000000}, {010, 020, 064, 073, 051, 076, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x04030001, 0x04050301, 0x00000000}, {073, 064, 076, 031, 040, 010, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {073, 064, 076, 031, 020, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {032, 062, 020, 073, 040, 076, 051, 064, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {032, 064, 010, 062, 051, 076, 073, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {031, 010, 040, 064, 073, 076, 032, 062, 020, 000, 000, 000},
        5, 9
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {062, 031, 032, 064, 073, 076, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04000103, 0x01050303, 0x00000000}, {031, 076, 032, 040, 051, 064, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {032, 051, 076, 031, 064, 010, 020, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {064, 010, 040, 076, 032, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {064, 032, 020, 076, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {040, 076, 051, 064, 031, 062, 020, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {010, 031, 051, 062, 064, 076, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {064, 010, 040, 076, 020, 062, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {064, 076, 062, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000200, 0x00000000, 0x00000000}, {062, 073, 075, 064, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {062, 073, 075, 064, 040, 010, 020, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x05040302, 0x00000006, 0x00000000}, {075, 064, 062, 073, 031, 010, 051, 000, 000, 000, 000, 000},
        3, 7
    },
    {
        {0x00020100, 0x05040302, 0x07050606, 0x00000000}, {073, 075, 064, 062, 051, 031, 040, 020, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {032, 073, 075, 020, 064, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x03000302, 0x02030504, 0x00000000}, {040, 010, 032, 075, 064, 073, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {051, 031, 010, 075, 020, 073, 064, 032, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {051, 032, 040, 031, 064, 073, 075, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {062, 032, 031, 064, 075, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {010, 020, 040, 031, 064, 032, 075, 062, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x03000302, 0x05020304, 0x00000000}, {051, 075, 064, 032, 010, 062, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {064, 032, 075, 062, 051, 020, 040, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x02020100, 0x00000301, 0x00000000, 0x00000000}, {031, 075, 020, 064, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {010, 064, 040, 031, 075, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {051, 020, 010, 075, 064, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {051, 064, 040, 075, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {075, 054, 040, 073, 062, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x03000302, 0x05020304, 0x00000000}, {075, 054, 010, 062, 073, 020, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x06030504, 0x05070404, 0x00000000}, {010, 051, 031, 040, 073, 054, 062, 075, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {073, 054, 062, 075, 020, 051, 031, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x02030200, 0x05040304, 0x00000000}, {032, 073, 075, 020, 054, 040, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {075, 032, 073, 054, 010, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {020, 032, 073, 075, 040, 054, 010, 051, 031, 000, 000, 000},
        5, 9
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {075, 032, 073, 054, 031, 051, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x03000302, 0x02030504, 0x00000000}, {032, 031, 075, 040, 062, 054, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {010, 062, 054, 020, 075, 032, 031, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {010, 075, 032, 051, 062, 054, 040, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {051, 075, 054, 032, 020, 062, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {040, 075, 054, 020, 031, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {010, 075, 054, 031, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {040, 075, 054, 020, 051, 010, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {051, 075, 054, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x00020100, 0x02030302, 0x00000004, 0x00000000}, {054, 064, 062, 051, 073, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x04050504, 0x07060506, 0x00000000}, {010, 020, 040, 054, 064, 051, 062, 073, 000, 000, 000, 000},
        4, 8
    },
    {
        {0x00020100, 0x04000103, 0x03010503, 0x00000000}, {031, 062, 073, 054, 010, 064, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {020, 054, 031, 040, 073, 064, 062, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x03000302, 0x05020304, 0x00000000}, {032, 073, 051, 064, 020, 054, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {051, 064, 073, 054, 032, 040, 010, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x00020100, 0x01020103, 0x01060504, 0x00060104}, {020, 073, 064, 032, 054, 031, 010, 000, 000, 000, 000, 000},
        5, 7
    },
    {
        {0x03020100, 0x00000504, 0x00000000, 0x00000000}, {031, 032, 073, 040, 054, 064, 000, 000, 000, 000, 000, 000},
        2, 6
    },
    {
        {0x03020100, 0x02030200, 0x05040304, 0x00000000}, {054, 064, 062, 051, 032, 031, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x03000302, 0x03040504, 0x00080706}, {051, 054, 064, 062, 031, 032, 010, 020, 040, 000, 000, 000},
        5, 9
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {062, 054, 064, 032, 010, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {062, 054, 064, 032, 040, 020, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {054, 031, 051, 064, 020, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {054, 031, 051, 064, 010, 040, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {054, 020, 010, 064, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {054, 064, 040, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x02020100, 0x00000301, 0x00000000, 0x00000000}, {051, 040, 073, 062, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {020, 051, 010, 062, 073, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {010, 073, 031, 040, 062, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {020, 073, 031, 062, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {032, 040, 020, 073, 051, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {051, 032, 073, 010, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {032, 040, 020, 073, 010, 031, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {031, 032, 073, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x00020100, 0x04030103, 0x00000001, 0x00000000}, {031, 062, 032, 051, 040, 000, 000, 000, 000, 000, 000, 000},
        3, 5
    },
    {
        {0x00020100, 0x01040103, 0x03010505, 0x00000000}, {020, 051, 010, 062, 031, 032, 000, 000, 000, 000, 000, 000},
        4, 6
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {010, 062, 032, 040, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {020, 062, 032, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x03020100, 0x00000001, 0x00000000, 0x00000000}, {031, 040, 020, 051, 000, 000, 000, 000, 000, 000, 000, 000},
        2, 4
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {010, 031, 051, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x00020100, 0x00000000, 0x00000000, 0x00000000}, {010, 040, 020, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        1, 3
    },
    {
        {0x00000000, 0x00000000, 0x00000000, 0x00000000}, {000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000, 000},
        0, 0
    },
};
