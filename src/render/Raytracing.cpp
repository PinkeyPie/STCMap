#include "Raytracing.h"
#include "../directx/DxContext.h"
#include "../directx/DxCommandList.h"

#include <dxcapi.h>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
    void ReportShaderCompileError(Com<IDxcBlobEncoding> blob) {
        char infoLog[2048];
        memcpy(infoLog, blob->GetBufferPointer(), sizeof(infoLog) - 1);
        std::cerr << "Error: " << infoLog << std::endl;
    }

    Com<IDxcBlob> CompileLibrary(const std::wstring& filename, const std::vector<const wchar*>& shaderNameDefines) {
        Com<IDxcCompiler> compiler;
        Com<IDxcLibrary> library;
        Com<IDxcIncludeHandler> includeHandler;
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf())));
        ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf())));
        ThrowIfFailed(library->CreateIncludeHandler(includeHandler.GetAddressOf()));

        std::ifstream stream(filename);
        if (not stream.is_open()) {
            return 0;
        }
        std::stringstream ss; ss << stream.rdbuf();
        std::string source = ss.str();

        // Create blob from the string.
        Com<IDxcBlobEncoding> textBlob;
        ThrowIfFailed(library->CreateBlobWithEncodingFromPinned((LPBYTE)source.c_str(), (uint32)source.length(), 0, textBlob.GetAddressOf()));

        std::wstring wfilename(filename.begin(), filename.end());

        std::vector<DxcDefine> defines;

        std::vector<std::wstring> valueStrings;
        valueStrings.reserve(1 + shaderNameDefines.size()); // Important. We pull out raw char pointers from these, so the vector should not reallocate.

        valueStrings.push_back(std::to_wstring(shaderNameDefines.size()));
        defines.push_back({L"NUM_RAY_TYPES", valueStrings.back().c_str()});

        for (uint32 i = 0; i < shaderNameDefines.size(); i++) {
            valueStrings.push_back(std::to_wstring(i));
            defines.push_back({shaderNameDefines[i], valueStrings.back().c_str()});
        }

        Com<IDxcOperationResult> operationResult;
        ThrowIfFailed(compiler->Compile(textBlob.Get(), wfilename.c_str(), L"", L"lib_6_3", 0, 0, defines.data(), (uint32)defines.size(), includeHandler.Get(), operationResult.GetAddressOf()));

        // Verify the result
        HRESULT resultCode;
        ThrowIfFailed(operationResult->GetStatus(&resultCode));
        if (FAILED(resultCode)) {
            Com<IDxcBlobEncoding> error;
            ThrowIfFailed(operationResult->GetErrorBuffer(&error));
            ReportShaderCompileError(error);
            assert(false);
            return 0;
        }

        Com<IDxcBlob> blob;
        ThrowIfFailed(operationResult->GetResult(&blob));

        return blob;
    }

    uint32 GetShaderBindingTableSize(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) {
        uint32 size = 0;
        for (uint32 i = 0; i < rootSignatureDesc.NumParameters; i++) {
            if (rootSignatureDesc.pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
                size += AlignTo(rootSignatureDesc.pParameters[i].Constants.Num32BitValues * 4, 8);
            }
            else {
                size += 8;
            }
        }
        return size;
    }
}

RaytracingBlasBuilder &RaytracingBlasBuilder::Push(Ptr<DxVertexBuffer> vertexBuffer, Ptr<DxIndexBuffer> indexBuffer, SubmeshInfo submesh, bool opaque, const trs &localTransform) {
    D3D12_RAYTRACING_GEOMETRY_DESC geomDesc;

    geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geomDesc.Flags = opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

    if (&localTransform == &trs::identity) {
        geomDesc.Triangles.Transform3x4 = UINT64_MAX;
    }
    else {
        geomDesc.Triangles.Transform3x4 = _localTransforms.size();
        _localTransforms.push_back(transpose(trsToMat4(localTransform)));
    }

    geomDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GpuVirtualAddress + (vertexBuffer->ElementSize * submesh.BaseVertex);
    geomDesc.Triangles.VertexBuffer.StrideInBytes = vertexBuffer->ElementSize;
    geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geomDesc.Triangles.VertexCount = submesh.NumVertices;

    geomDesc.Triangles.IndexBuffer = indexBuffer->GpuVirtualAddress + (indexBuffer->ElementSize * submesh.FirstTriangle * 3);
    geomDesc.Triangles.IndexFormat = DxIndexBuffer::GetIndexBufferFormat(indexBuffer->ElementSize);
    geomDesc.Triangles.IndexCount = submesh.NumTriangles * 3;

    _geometryDescs.push_back(geomDesc);
    _geometries.push_back({ERaytracingMeshGeometry, vertexBuffer, indexBuffer, submesh});

    return *this;
}

RaytracingBlasBuilder &RaytracingBlasBuilder::Push(const std::vector<BoundingBox> &boundingBoxes, bool opaque) {
    D3D12_RAYTRACING_GEOMETRY_DESC geomDesc;

    geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
    geomDesc.Flags = opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

    for (uint32 i = 0; i < (uint32)boundingBoxes.size(); i++) {
        D3D12_RAYTRACING_AABB& w = _aabbDescs.emplace_back();
        const BoundingBox& r = boundingBoxes[i];

        w.MinX = r.MinCorner.x;
        w.MinY = r.MinCorner.y;
        w.MinZ = r.MinCorner.z;

        w.MaxX = r.MaxCorner.x;
        w.MaxY = r.MaxCorner.y;
        w.MaxZ = r.MaxCorner.z;
    }

    geomDesc.AABBs.AABBCount = boundingBoxes.size();

    _geometryDescs.push_back(geomDesc);
    _geometries.push_back({ERaytracingProceduralGeometry});

    return *this;
}

Ptr<RaytracingBlas> RaytracingBlasBuilder::Finish(bool keepScratch) {
    DxContext& dxContext = DxContext::Instance();
    DxDynamicConstantBuffer localTransformsBuffer = {};
    if (not _localTransforms.empty()) {
        localTransformsBuffer = dxContext.UploadDynamicConstantBuffer(sizeof(mat4) * (uint32)_localTransforms.size(), _localTransforms.data());
    }

    DxDynamicConstantBuffer aabbBuffer = {};
    if (not _aabbDescs.empty()) {
        aabbBuffer = dxContext.UploadDynamicConstantBuffer(sizeof(D3D12_RAYTRACING_AABB) * (uint32)_aabbDescs.size(), _aabbDescs.data());
    }

    uint64 aabbOffset = 0;

    for (auto& desc : _geometryDescs) {
        if (desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES) {
            if (desc.Triangles.Transform3x4 == UINT64_MAX) {
                desc.Triangles.Transform3x4 = 0;
            }
            else {
                assert(localTransformsBuffer.CpuPtr);
                desc.Triangles.Transform3x4 = localTransformsBuffer.GpuPtr + sizeof(mat4) * desc.Triangles.Transform3x4;
            }
        }
        else if (desc.Type == D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS) {
            assert(aabbBuffer.CpuPtr);
            desc.AABBs.AABBs.StartAddress = aabbBuffer.GpuPtr + sizeof(D3D12_RAYTRACING_AABB) * aabbOffset;
            desc.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
            aabbOffset += desc.AABBs.AABBCount;
        }
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    inputs.NumDescs = (uint32)_geometryDescs.size();
    inputs.pGeometryDescs = _geometryDescs.data();

    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

    // Allocate.
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    dxContext.GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    info.ScratchDataSizeInBytes = AlignTo(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    info.ResultDataMaxSizeInBytes = AlignTo(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    Ptr<RaytracingBlas> blas = MakePtr<RaytracingBlas>();
    blas->Scratch = DxBuffer::Create((uint32)info.ScratchDataSizeInBytes, 1, 0, true, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    blas->Blas = DxBuffer::Create((uint32)info.ResultDataMaxSizeInBytes, 1, 0, true, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    SET_NAME(blas->Scratch->Resource, "BLAS Scratch");
    SET_NAME(blas->Blas->Resource, "BLAS Result");

    blas->Geometries = std::move(_geometries);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = blas->Blas->GpuVirtualAddress;
    asDesc.ScratchAccelerationStructureData = blas->Scratch->GpuVirtualAddress;

    DxCommandList* cl = dxContext.GetFreeComputeCommandList(true);
    cl->CommandList()->BuildRaytracingAccelerationStructure(&asDesc, 0, 0);
    cl->UavBarrier(blas->Blas);
    dxContext.ExecuteCommandList(cl);

    if (!keepScratch)
    {
        blas->Scratch = 0;
    }

    return blas;
}

RaytracingPipelineBuilder::RaytracingPipelineBuilder(const wchar *shaderFilename, uint32 payloadSize, uint32 maxRecursionDepth, bool hasMeshGeometry, bool hasProceduralGeometry) {
    _shaderFilename = shaderFilename;
    _payloadSize = payloadSize;
    _maxRecursionDepth = maxRecursionDepth;
    _hasMeshGeometry = hasMeshGeometry;
    _hasProceduralGeometry = hasProceduralGeometry;

    assert(hasMeshGeometry or hasProceduralGeometry);
}

RaytracingPipelineBuilder::RaytracingRootSignature RaytracingPipelineBuilder::CreateRaytracingRootSignature(const D3D12_ROOT_SIGNATURE_DESC &desc) {
    RaytracingRootSignature result;
    result.RootSignature.Initialize(desc);
    result.RootSignaturePtr = result.RootSignature.RootSignature();
    return result;
}

RaytracingPipelineBuilder &RaytracingPipelineBuilder::GlobalRootSignature(D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc) {
    assert(not _globalRS.RootSignature.RootSignature());

    _globalRS = CreateRaytracingRootSignature(rootSignatureDesc);
    SET_NAME(_globalRS.RootSignature.RootSignature(), "Global raytracing root signature");

    auto& so = _subobjects[_numSubobjects++];
    so.pDesc = &_globalRS.RootSignaturePtr;
    so.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;

    return *this;
}

RaytracingPipelineBuilder &RaytracingPipelineBuilder::RayGen(const wchar *entryPoint, D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc) {
    assert(not _rayGenRS.RootSignature.RootSignature());

    D3D12_EXPORT_DESC& exp = _exports[_numExports++];
    exp.Name = entryPoint;
    exp.Flags = D3D12_EXPORT_FLAG_NONE;
    exp.ExportToRename = 0;

    _rayGenEntryPoint = entryPoint;

    rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
    _rayGenRS = CreateRaytracingRootSignature(rootSignatureDesc);
    SET_NAME(_rayGenRS.RootSignature.RootSignature(), "Local raytracing root signature");

    {
        auto& so = _subobjects[_numSubobjects++];
        so.pDesc = &_rayGenRS.RootSignaturePtr;
        so.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    }

    {
        auto& so = _subobjects[_numSubobjects++];
        auto& as = _associations[_numAssociations++];

        _stringBuffer[_numStrings++] = entryPoint;

        as.NumExports = 1;
        as.pExports = &_stringBuffer[_numStrings - 1];
        as.pSubobjectToAssociate = &_subobjects[_numSubobjects - 2];

        so.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        so.pDesc = &as;
    }

    _allExports.push_back(entryPoint);

    uint32 size = GetShaderBindingTableSize(rootSignatureDesc);
    _tableEntrySize = Max(size, _tableEntrySize);

    return *this;
}

RaytracingPipelineBuilder &RaytracingPipelineBuilder::HitGroup(const wchar *groupName, const wchar *miss, RaytracingMeshHitGroup mesh, D3D12_ROOT_SIGNATURE_DESC meshRootSignatureDesc, RaytracingProceduralHitGroup procedural, D3D12_ROOT_SIGNATURE_DESC proceduralRootSignatureDesc) {
    auto exportEntryPoint = [this](const wchar* entryPoint) {
		D3D12_EXPORT_DESC& exp = _exports[_numExports++];
		exp.Name = entryPoint;
		exp.Flags = D3D12_EXPORT_FLAG_NONE;
		exp.ExportToRename = 0;

		_allExports.push_back(entryPoint);
	};

	auto createLocalRootSignature = [this](D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc, const wchar* closestHit, const wchar* anyHit, const wchar* intersection) {
		if (rootSignatureDesc.NumParameters > 0) {
			rootSignatureDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			_rootSignatures[_numRootSignatures++] = CreateRaytracingRootSignature(rootSignatureDesc);

			{
				auto& so = _subobjects[_numSubobjects++];
				so.pDesc = &_rootSignatures[_numRootSignatures - 1].RootSignaturePtr;
				so.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
			}

			{
				auto& so = _subobjects[_numSubobjects++];
				auto& as = _associations[_numAssociations++];

				const wchar** entryPoints = &_stringBuffer[_numStrings];
				uint32 numEntryPoints = 0;

				if (closestHit) { entryPoints[numEntryPoints++] = closestHit; }
				if (anyHit) { entryPoints[numEntryPoints++] = anyHit; }
				if (intersection) { entryPoints[numEntryPoints++] = intersection; }

				_numStrings += numEntryPoints;

				as.NumExports = numEntryPoints;
				as.pExports = entryPoints;
				as.pSubobjectToAssociate = &_subobjects[_numSubobjects - 2];

				so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
				so.pDesc = &as;
			}

			uint32 size = GetShaderBindingTableSize(rootSignatureDesc);
			_tableEntrySize = Max(size, _tableEntrySize);
		}
	};

	if (_hasMeshGeometry) {
		_groupNameStorage[_groupNameStoragePtr++] = std::wstring(groupName) + L"_MESH";
		const wchar* name = _groupNameStorage[_groupNameStoragePtr - 1].c_str();

		D3D12_HIT_GROUP_DESC& hitGroup = _hitGroups[_numHitGroups++];

		hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		hitGroup.AnyHitShaderImport = mesh.AnyHit;
		hitGroup.ClosestHitShaderImport = mesh.CloseHit;
		hitGroup.IntersectionShaderImport = 0;
		hitGroup.HitGroupExport = name;

		auto& so = _subobjects[_numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		so.pDesc = &hitGroup;

		if (mesh.CloseHit) {
			exportEntryPoint(mesh.CloseHit);
		}
		if (mesh.AnyHit) {
			exportEntryPoint(mesh.AnyHit);
		}

		createLocalRootSignature(meshRootSignatureDesc, mesh.CloseHit, mesh.AnyHit, 0);
	}

	if (_hasProceduralGeometry) {
		_groupNameStorage[_groupNameStoragePtr++] = std::wstring(groupName) + L"_PROC";
		const wchar* name = _groupNameStorage[_groupNameStoragePtr - 1].c_str();

		D3D12_HIT_GROUP_DESC& hitGroup = _hitGroups[_numHitGroups++];

		hitGroup.Type = D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE;
		hitGroup.AnyHitShaderImport = procedural.AnyHit;
		hitGroup.ClosestHitShaderImport = procedural.ClosestHit;
		hitGroup.IntersectionShaderImport = procedural.Intersection;
		hitGroup.HitGroupExport = name;

		auto& so = _subobjects[_numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		so.pDesc = &hitGroup;

		assert(procedural.Intersection);

		if (procedural.ClosestHit) {
			exportEntryPoint(procedural.ClosestHit);
		}
		if (procedural.Intersection) {
			exportEntryPoint(procedural.Intersection);
		}
		if (procedural.AnyHit) {
			exportEntryPoint(procedural.AnyHit);
		}

		createLocalRootSignature(meshRootSignatureDesc, procedural.ClosestHit, procedural.AnyHit, procedural.Intersection);
	}

	exportEntryPoint(miss);
	_emptyAssociations.push_back(miss);
	_missEntryPoints.push_back(miss);

	_shaderNameDefines.push_back(groupName);

	return *this;
}

DxRaytracingPipeline RaytracingPipelineBuilder::Finish() {
	assert(_rayGenRS.RootSignature.RootSignature());
	assert(_globalRS.RootSignature.RootSignature());

	auto shaderBlob = CompileLibrary(_shaderFilename, _shaderNameDefines);

	D3D12_DXIL_LIBRARY_DESC dxilLibDesc;
	dxilLibDesc.DXILLibrary.pShaderBytecode = shaderBlob->GetBufferPointer();
	dxilLibDesc.DXILLibrary.BytecodeLength = shaderBlob->GetBufferSize();
	dxilLibDesc.NumExports = _numExports;
	dxilLibDesc.pExports = _exports;

	D3D12_ROOT_SIGNATURE_DESC emptyRootSignatureDesc = {};
	emptyRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	RaytracingRootSignature emptyRootSignature = CreateRaytracingRootSignature(emptyRootSignatureDesc);

	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig;
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig;

	{
		D3D12_STATE_SUBOBJECT& so = _subobjects[_numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		so.pDesc = &dxilLibDesc;
	}

	{
		D3D12_STATE_SUBOBJECT& so = _subobjects[_numSubobjects++];
		so.pDesc = &emptyRootSignature.RootSignaturePtr;
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	}

	{
		auto& so = _subobjects[_numSubobjects++];
		auto& as = _associations[_numAssociations++];

		as.NumExports = (uint32)_emptyAssociations.size();
		as.pExports = _emptyAssociations.data();
		as.pSubobjectToAssociate = &_subobjects[_numSubobjects - 2];

		so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		so.pDesc = &as;
	}

	{
		shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2; // 2 floats for the BuiltInTriangleIntersectionAttributes.
		shaderConfig.MaxPayloadSizeInBytes = _payloadSize;

		auto& so = _subobjects[_numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		so.pDesc = &shaderConfig;
	}

	{
		auto& so = _subobjects[_numSubobjects++];
		auto& as = _associations[_numAssociations++];

		as.NumExports = (uint32)_allExports.size();
		as.pExports = _allExports.data();
		as.pSubobjectToAssociate = &_subobjects[_numSubobjects - 2];

		so.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
		so.pDesc = &as;
	}

	{
		pipelineConfig.MaxTraceRecursionDepth = _maxRecursionDepth;

		auto& so = _subobjects[_numSubobjects++];
		so.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		so.pDesc = &pipelineConfig;
	}


	D3D12_STATE_OBJECT_DESC desc;
	desc.NumSubobjects = _numSubobjects;
	desc.pSubobjects = _subobjects;
	desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	DxContext& dxContext = DxContext::Instance();
	DxRaytracingPipeline result;
	ThrowIfFailed(dxContext.GetDevice()->CreateStateObject(&desc, IID_PPV_ARGS(&result.Pipeline)));
	result.RootSignature = _globalRS.RootSignature;


	for (uint32 i = 0; i < _numRootSignatures; ++i) {
		_rootSignatures[i].RootSignature.Free();
	}
	_rayGenRS.RootSignature.Free();


	Com<ID3D12StateObjectProperties> rtsoProps;
	result.Pipeline->QueryInterface(IID_PPV_ARGS(&rtsoProps));


	{
		auto& shaderBindingTableDesc = result.ShaderBindingTableDesc;
		_tableEntrySize += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		shaderBindingTableDesc.EntrySize = (uint32)AlignTo(_tableEntrySize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		uint32 numGeometryTypes = _hasMeshGeometry + _hasProceduralGeometry;

		uint32 numRaygenShaderEntries = 1;
		uint32 numMissShaderEntries = _numHitGroups / numGeometryTypes;

		shaderBindingTableDesc.RayGen = rtsoProps->GetShaderIdentifier(_rayGenEntryPoint);

		uint32 numUniqueGroups = (uint32)_missEntryPoints.size();
		assert(numUniqueGroups * numGeometryTypes == _numHitGroups);

		for (uint32 i = 0; i < numUniqueGroups; ++i)
		{
			shaderBindingTableDesc.Miss.push_back(rtsoProps->GetShaderIdentifier(_missEntryPoints[i]));

			RaytracingShader& shader = shaderBindingTableDesc.HitGroups.emplace_back();
			shader = { };
			if (_hasMeshGeometry)
			{
				shader.Mesh = rtsoProps->GetShaderIdentifier(_hitGroups[numGeometryTypes * i].HitGroupExport);
			}
			if (_hasProceduralGeometry)
			{
				shader.Procedural = rtsoProps->GetShaderIdentifier(_hitGroups[numGeometryTypes * i + numGeometryTypes - 1].HitGroupExport);
			}
		}


		shaderBindingTableDesc.RayGenOffset = 0;
		shaderBindingTableDesc.MissOffset = shaderBindingTableDesc.RayGenOffset + (uint32)AlignTo(numRaygenShaderEntries * _tableEntrySize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
		shaderBindingTableDesc.HitOffset = shaderBindingTableDesc.MissOffset + (uint32)AlignTo(numMissShaderEntries * _tableEntrySize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
	}

	return result;
}

