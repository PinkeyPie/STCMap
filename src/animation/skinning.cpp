#include "skinning.h"
#include "../directx/DxCommandList.h"
#include "../directx/DxPipeline.h"
#include "skinning_rs.hlsli"

#define MAX_NUM_SKINNING_MATRICES_PER_FRAME 4096
#define MAX_NUM_SKINNED_VERTICES_PER_FRAME (1024 * 256)

static Ptr<DxBuffer> SkinningMatricesBuffer; // Buffered frames are in a single dx_buffer.

static uint32 CurrentSkinnedVertexBuffer;
static Ptr<DxVertexBuffer> SkinnedVertexBuffer[2]; // We have two of these, so that we can compute screen space velocities.

static DxPipeline SkinningPipeline;


struct SkinningCall {
	Ptr<DxVertexBuffer> VertexBuffer;
	VertexRange Range;
	uint32 JointOffset;
	uint32 NumJoints;
	uint32 VertexOffset;
};

static std::vector<SkinningCall> Calls;
static std::vector<mat4> SkinningMatrices;
static uint32 TotalNumVertices;


void InitializeSkinning() {
	SkinningMatricesBuffer = DxBuffer::CreateUpload(sizeof(mat4), MAX_NUM_SKINNING_MATRICES_PER_FRAME * NUM_BUFFERED_FRAMES, 0);

	for (uint32 i = 0; i < 2; ++i) {
		SkinnedVertexBuffer[i] = DxVertexBuffer::Create(GetVertexSize(EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithUvs | EMeshCreationFlagsWithNormals | EMeshCreationFlagsWithTangents),
			MAX_NUM_SKINNED_VERTICES_PER_FRAME, 0, true);
	}

	SkinningPipeline = DxPipelineFactory::Instance()->CreateReloadablePipeline("skinning_cs");

	SkinningMatrices.reserve(MAX_NUM_SKINNING_MATRICES_PER_FRAME);
}

std::tuple<Ptr<DxVertexBuffer>, VertexRange, mat4*> SkinObject(const Ptr<DxVertexBuffer>& vertexBuffer, VertexRange range, uint32 numJoints) {
	uint32 offset = (uint32)SkinningMatrices.size();

	assert(offset + numJoints <= MAX_NUM_SKINNING_MATRICES_PER_FRAME);

	SkinningMatrices.resize(SkinningMatrices.size() + numJoints);

	Calls.push_back({
		vertexBuffer,
		range,
		offset,
		numJoints,
		TotalNumVertices
	});

	VertexRange resultRange;
	resultRange.NumVertices = range.NumVertices;
	resultRange.FirstVertex = TotalNumVertices;

	TotalNumVertices += range.NumVertices;

	assert(TotalNumVertices <= MAX_NUM_SKINNED_VERTICES_PER_FRAME);

	return { SkinnedVertexBuffer[CurrentSkinnedVertexBuffer], resultRange, SkinningMatrices.data() + offset };
}

std::tuple<Ptr<DxVertexBuffer>, uint32, mat4*> SkinObject(const Ptr<DxVertexBuffer>& vertexBuffer, uint32 numJoints) {
	auto [vb, range, mats] = SkinObject(vertexBuffer, VertexRange{ 0, vertexBuffer->ElementCount }, numJoints);

	return { vb, range.FirstVertex, mats };
}

std::tuple<Ptr<DxVertexBuffer>, SubmeshInfo, mat4*> SkinObject(const Ptr<DxVertexBuffer>& vertexBuffer, SubmeshInfo submesh, uint32 numJoints) {
	auto [vb, range, mats] = SkinObject(vertexBuffer, VertexRange{ submesh.BaseVertex, submesh.NumVertices }, numJoints);

	SubmeshInfo resultInfo;
	resultInfo.FirstTriangle = submesh.FirstTriangle;
	resultInfo.NumTriangles = submesh.NumTriangles;
	resultInfo.BaseVertex = range.FirstVertex;
	resultInfo.NumVertices = range.NumVertices;

	return { vb, resultInfo, mats };
}

bool PerformSkinning() {
	DxContext& dxContext = DxContext::Instance();
	bool result = false;
	if (Calls.size() > 0)
	{
		DxCommandList* cl = dxContext.GetFreeComputeCommandList(true);

		uint32 matrixOffset = dxContext.BufferedFrameId() * MAX_NUM_SKINNING_MATRICES_PER_FRAME;

		mat4* mats = (mat4*)SkinningMatricesBuffer->Map(false);
		memcpy(mats + matrixOffset, SkinningMatrices.data(), sizeof(mat4) * SkinningMatrices.size());
		SkinningMatricesBuffer->Unmap(true, MapRange{matrixOffset, (uint32)SkinningMatrices.size()});


		cl->SetPipelineState(*SkinningPipeline.Pipeline);
		cl->SetComputeRootSignature(*SkinningPipeline.RootSignature);

		cl->SetRootComputeSRV(SkinningRsMatruces, SkinningMatricesBuffer->GpuVirtualAddress + sizeof(mat4) * matrixOffset);
		cl->SetRootComputeUAV(SkinningRsOutput, SkinnedVertexBuffer[CurrentSkinnedVertexBuffer]->GpuVirtualAddress);

		for (const auto& c : Calls)
		{
			cl->SetRootComputeSRV(SkinningRsInputVertexBuffer, c.VertexBuffer->GpuVirtualAddress);
			cl->SetCompute32BitConstants(SkinningRsCb, SkinningCb{ c.JointOffset, c.NumJoints, c.Range.FirstVertex, c.Range.NumVertices, c.VertexOffset });
			cl->Dispatch(bucketize(c.Range.NumVertices, 512));
		}

		cl->UavBarrier(SkinnedVertexBuffer[CurrentSkinnedVertexBuffer]);

		dxContext.ExecuteCommandList(cl);

		result = true;
	}

	CurrentSkinnedVertexBuffer = 1 - CurrentSkinnedVertexBuffer;
	Calls.clear();
	SkinningMatrices.clear();
	TotalNumVertices = 0;

	return result;
}
