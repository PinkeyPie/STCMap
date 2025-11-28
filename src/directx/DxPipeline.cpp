#include "DxPipeline.h"

#include "../pch.h"
#include "DxPipeline.h"

#include <corecrt_io.h>

#include "../core/threading.h"

#include <unordered_map>
#include <set>
#include <deque>
#include <filesystem>
#include <iostream>
#include <ppl.h>

namespace fs = std::filesystem;

static const wchar* shaderDir = L"shaders//";

DxPipelineFactory* DxPipelineFactory::_instance = new DxPipelineFactory{};

namespace {
	std::wstring StringToWideString(const std::string& s) {
		return std::wstring(s.begin(), s.end());
	}

	void CopyRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC* desc, DxRootSignature& result) {
		uint32 numDescriptorTables = 0;
		for (uint32 i = 0; i < desc->NumParameters; i++) {

		}
	}
}

void DxPipelineFactory::ReloadablePipelineState::Initialize(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, const GraphicsPipelineFiles &files, DxRootSignature* rootSignature) {
	Type = EPipelineTypeGraphics;
	DescriptionType = EDescTypeStruct;
	GraphicsDesc = desc;
	GraphicsFiles = files;
	RootSignature = rootSignature;

	assert(desc.InputLayout.NumElements <= arraysize(InputLayout));

	memcpy(InputLayout, desc.InputLayout.pInputElementDescs, sizeof(D3D12_INPUT_ELEMENT_DESC) * desc.InputLayout.NumElements);
	GraphicsDesc.InputLayout.pInputElementDescs = InputLayout;

	if (desc.InputLayout.NumElements == 0) {
		GraphicsDesc.InputLayout.pInputElementDescs = nullptr;
	}
}

void DxPipelineFactory::ReloadablePipelineState::Initialize(const D3D12_PIPELINE_STATE_STREAM_DESC &desc, DxPipelineStreamBase *stream, const GraphicsPipelineFiles &files, DxRootSignature *rootSignature) {
	Type = EPipelineTypeGraphics;
	DescriptionType = EDescTypeStream;
	StreamDesc = desc;
	Stream = stream;
	GraphicsFiles = files;
	RootSignature = rootSignature;

	// TODO: Handle input layout. For now we expect the user to use one of the globally defined formats.
}


void DxPipelineFactory::ReloadablePipelineState::Initialize(const char *file, DxRootSignature* rootSignature) {
	Type = EPipelineTypeCompute;
	ComputeDesc = {};
	ComputeFile = file;
	RootSignature = rootSignature;
}

DxPipelineFactory::ReloadableRootSignature* DxPipelineFactory::PushBlob(const char* filename, ReloadablePipelineState* pipelineIndex, bool isRootSignature) {
	ReloadableRootSignature* result = nullptr;

	if (filename) {
		auto it = _shaderBlobs.find(filename);
		if (it == _shaderBlobs.end()) {
			// New file
			std::wstring filepath = shaderDir + StringToWideString(filename) + L".cso";

			DxBlob blob;
			ThrowIfFailed(D3DReadFileToBlob(filepath.c_str(), blob.GetAddressOf()));

			if (isRootSignature) {
				_rootSignatureFromFiles.push_back({filename, {}});
				result = &_rootSignatureFromFiles.back();
			}

			_mutex.lock();
			_shaderBlobs[filename] = {.Blob = blob, .UsedByPipelines = {pipelineIndex} };
			_mutex.unlock();
		}
		else {
			// Already used

			_mutex.lock();
			it->second.UsedByPipelines.insert(pipelineIndex);
			_mutex.unlock();

			if (isRootSignature) {
				if (!it->second.RootSignature) {
					_rootSignatureFromFiles.push_back({filename, {}});
					it->second.RootSignature = &_rootSignatureFromFiles.back();
				}

				result = it->second.RootSignature;
			}
		}
	}

	return result;
}

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const GraphicsPipelineFiles& files, DxRootSignature userRootSignature) {
	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	PushBlob(files.VS, &state);
	PushBlob(files.PS, &state);
	PushBlob(files.GS, &state);
	PushBlob(files.DS, &state);
	PushBlob(files.HS, &state);

	assert(not files.MS);
	assert(not files.AS);

	_userRootSignatures.push_back(userRootSignature);
	DxRootSignature* rootSignature = &_userRootSignatures.back();
	_userRootSignatures.back() = userRootSignature; // Fuck you

	state.Initialize(desc, files, rootSignature);

	DxPipeline result = {&state.Pipeline, rootSignature};
	return result;
}

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, const GraphicsPipelineFiles &files, ERsFile rootSignatureFile) {
	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	ReloadableRootSignature* reloadableRs = PushBlob(files.shaders[rootSignatureFile], &state, true);
	PushBlob(files.VS, &state);
	PushBlob(files.PS, &state);
	PushBlob(files.GS, &state);
	PushBlob(files.DS, &state);
	PushBlob(files.HS, &state);

	assert(not files.MS);
	assert(not files.AS);

	DxRootSignature* rootSignature = &reloadableRs->RootSignature;

	state.Initialize(desc, files, rootSignature);

	DxPipeline result = {&state.Pipeline, rootSignature};
	return result;
}

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const char *csFile, DxRootSignature userRootSignature) {
	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	PushBlob(csFile, &state);

	_userRootSignatures.push_back(userRootSignature);
	DxRootSignature* rootSignature = &_userRootSignatures.back();
	_userRootSignatures.back() = userRootSignature; // Fuck. You

	state.Initialize(csFile, rootSignature);

	DxPipeline result = { &state.Pipeline, rootSignature };
	return result;
}

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const char *csFile) {
	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	ReloadableRootSignature* reloadableRs = PushBlob(csFile, &state, true);
	PushBlob(csFile, &state);

	DxRootSignature* rootSignature = &reloadableRs->RootSignature;

	state.Initialize(csFile, rootSignature);

	DxPipeline result = {&state.Pipeline, rootSignature};
	return result;
}

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const D3D12_PIPELINE_STATE_STREAM_DESC &desc, DxPipelineStreamBase *stream, const GraphicsPipelineFiles &files, DxRootSignature userRootSignature) {
	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	PushBlob(files.VS, &state);
	PushBlob(files.HS, &state);
	PushBlob(files.DS, &state);
	PushBlob(files.GS, &state);
	PushBlob(files.PS, &state);
	PushBlob(files.AS, &state);
	PushBlob(files.MS, &state);

	_userRootSignatures.push_back(userRootSignature);
	DxRootSignature* rootSignature = &_userRootSignatures.back();
	_userRootSignatures.back() = userRootSignature;

	state.Initialize(desc, stream, files, rootSignature);

	DxPipeline result = { &state.Pipeline, rootSignature };
	return result;
}

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const D3D12_PIPELINE_STATE_STREAM_DESC &desc, DxPipelineStreamBase *stream, const GraphicsPipelineFiles &files, ERsFile rootSignatureFile) {
	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	ReloadableRootSignature* reloadableRS = PushBlob(files.shaders[rootSignatureFile], &state, true);
	PushBlob(files.VS, &state);
	PushBlob(files.HS, &state);
	PushBlob(files.DS, &state);
	PushBlob(files.GS, &state);
	PushBlob(files.PS, &state);
	PushBlob(files.AS, &state);
	PushBlob(files.MS, &state);

	DxRootSignature* rootSignature = &reloadableRS->RootSignature;

	state.Initialize(desc, stream, files, rootSignature);

	DxPipeline result = { &state.Pipeline, rootSignature };
	return result;
}


void DxPipelineFactory::LoadRootSignature(ReloadableRootSignature &r) {
	DxBlob rs = _shaderBlobs[r.File].Blob;

	DxContext::Instance().Retire(r.RootSignature.RootSignature());
	r.RootSignature.Free();
	r.RootSignature.Initialize(rs);
}

void DxPipelineFactory::LoadPipeline(ReloadablePipelineState& p) {
	DxContext& dxContext = DxContext::Instance();
	dxContext.Retire(p.Pipeline);

	if (p.Type == EPipelineTypeGraphics) {
		if (p.DescriptionType == EDescTypeStruct) {
			if (p.GraphicsFiles.VS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.VS].Blob;
				p.GraphicsDesc.VS = CD3DX12_SHADER_BYTECODE(shader.Get());
			}
			if (p.GraphicsFiles.PS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.PS].Blob;
				p.GraphicsDesc.PS = CD3DX12_SHADER_BYTECODE(shader.Get());
			}
			if (p.GraphicsFiles.GS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.GS].Blob;
				p.GraphicsDesc.GS = CD3DX12_SHADER_BYTECODE(shader.Get());
			}
			if (p.GraphicsFiles.DS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.DS].Blob;
				p.GraphicsDesc.DS = CD3DX12_SHADER_BYTECODE(shader.Get());
			}
			if (p.GraphicsFiles.HS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.HS].Blob;
				p.GraphicsDesc.HS = CD3DX12_SHADER_BYTECODE(shader.Get());
			}

			p.GraphicsDesc.pRootSignature = p.RootSignature->RootSignature();
			ThrowIfFailed(dxContext.GetDevice()->CreateGraphicsPipelineState(&p.GraphicsDesc, IID_PPV_ARGS(p.Pipeline.GetAddressOf())));
		}
		else {
			if (p.GraphicsFiles.VS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.VS].Blob;
				p.Stream->SetVertexShader(shader);
			}
			if (p.GraphicsFiles.HS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.HS].Blob;
				p.Stream->SetHullShader(shader);
			}
			if (p.GraphicsFiles.DS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.DS].Blob;
				p.Stream->SetDomainShader(shader);
			}
			if (p.GraphicsFiles.GS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.GS].Blob;
				p.Stream->SetGeometryShader(shader);
			}
			if (p.GraphicsFiles.PS) {
				DxBlob shader = _shaderBlobs[p.GraphicsFiles.PS].Blob;
				p.Stream->SetPixelShader(shader);
			}

			p.Stream->SetRootSignature(*p.RootSignature);
			ThrowIfFailed(dxContext.GetDevice()->CreatePipelineState(&p.StreamDesc, IID_PPV_ARGS(&p.Pipeline)));
		}
	}
	else {
		DxBlob shader = _shaderBlobs[p.ComputeFile].Blob;
		p.ComputeDesc.CS = CD3DX12_SHADER_BYTECODE(shader.Get());

		p.ComputeDesc.pRootSignature = p.RootSignature->RootSignature();
		ThrowIfFailed(dxContext.GetDevice()->CreateComputePipelineState(&p.ComputeDesc, IID_PPV_ARGS(p.Pipeline.GetAddressOf())));
	}
}

void DxPipelineFactory::CreateAllPendingReloadablePipelines() {
	static int rsOffset = 0;
	static int pipelineOffset = 0;

#if 1
	concurrency::parallel_for(rsOffset, (int)_rootSignatureFromFiles.size(), [&](int i) {
		LoadRootSignature(_rootSignatureFromFiles[i]);
	});

	concurrency::parallel_for(pipelineOffset, (int)_pipelines.size(), [&](int i) {
		LoadPipeline(_pipelines[i]);
	});
#else
	for (int i = 0; i < _rootSignatureFromFiles.size(); i++) {
		LoadRootSignature(_rootSignatureFromFiles[i]);
	}

	for (int i = 0; i < _pipelines.size(); i++) {
		LoadPipeline(_pipelines[i]);
	}
#endif

	rsOffset = (int)_rootSignatureFromFiles.size();
	pipelineOffset = (int)_pipelines.size();

	std::thread fileWatcher([&]() {
		CheckForFileChanges();
	});
	fileWatcher.detach();
}

void DxPipelineFactory::CheckForChangedPipelines() {
	_mutex.lock();
	concurrency::parallel_for(0, (int)_dirtyRootSignatures.size(), [&](int i) {
		LoadRootSignature(*_dirtyRootSignatures[i]);
	});
	concurrency::parallel_for(0, (int)_dirtyPipelines.size(), [&](int i) {
		LoadPipeline(*_dirtyPipelines[i]);
	});
	_dirtyPipelines.clear();
	_dirtyPipelines.clear();
	_mutex.unlock();
}


static bool FileIsLocked(const wchar* filename) {
	HANDLE fileHandle = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (filename == INVALID_HANDLE_VALUE) {
		return true;
	}
	CloseHandle(fileHandle);
	return false;
}

DWORD DxPipelineFactory::CheckForFileChanges() {
	HANDLE directoryHandle;
	OVERLAPPED overlapped;

	uint8 buffer[1024] = {};

	directoryHandle = CreateFileW(
		shaderDir,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL
	);

	if (directoryHandle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Monitor directory failed.\n");
		return 1;
	}

	overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	ResetEvent(overlapped.hEvent);

	DWORD eventName = FILE_NOTIFY_CHANGE_LAST_WRITE;

	DWORD error = ReadDirectoryChangesW(directoryHandle,
		buffer, sizeof(buffer), TRUE,
		eventName,
		NULL, &overlapped, NULL);

	fs::path lastChangedPath = "";
	fs::file_time_type lastChangedPathTimeStamp;

	while (true) {
		DWORD result = WaitForSingleObject(overlapped.hEvent, INFINITE);

		DWORD dw;
		if (not GetOverlappedResult(directoryHandle, &overlapped, &dw, FALSE) or dw == 0) {
			fprintf(stderr, "Get overlapped result failed.\n");
			return 1;
		}

		FILE_NOTIFY_INFORMATION* fileNotify;

		DWORD offset = 0;

		do {
			fileNotify = (FILE_NOTIFY_INFORMATION*)(&buffer[offset]);

			if (fileNotify->Action == FILE_ACTION_MODIFIED) {
				char filename[MAX_PATH];
				int ret = WideCharToMultiByte(CP_ACP, 0, fileNotify->FileName,
					fileNotify->FileNameLength / sizeof(WCHAR),
					filename, MAX_PATH, NULL, NULL);

				filename[fileNotify->FileNameLength / sizeof(WCHAR)] = 0;

				fs::path changedPath = (shaderDir / fs::path(filename)).lexically_normal();
				auto changedPathWriteTime = fs::last_write_time(changedPath);

				// The filesystem usually sends multiple notifications for changed files, since the file is first written, then metadata is changed etc.
				// This check prevents these notifications if they are too close together in time.
				// This is a pretty crude fix. In this setup files should not change at the same time, since we only ever track one file.
				if (changedPath == lastChangedPath and
					std::chrono::duration_cast<std::chrono::milliseconds>(changedPathWriteTime - lastChangedPathTimeStamp).count() < 200) {
					lastChangedPath = changedPath;
					lastChangedPathTimeStamp = changedPathWriteTime;
					break;
				}

				bool isFile = not fs::is_directory(changedPath);

				if (isFile) {
					_mutex.lock();
					auto it = _shaderBlobs.find(changedPath.stem().string());
					if (it != _shaderBlobs.end()) {
						_mutex.unlock();
						auto wPath = changedPath.wstring();
						while (FileIsLocked(wPath.c_str())) {}

						std::cout << "Reloading shader blob " << changedPath << std::endl;
						DxBlob blob;
						ThrowIfFailed(D3DReadFileToBlob(changedPath.wstring().c_str(), blob.GetAddressOf()));

						_mutex.lock();
						it->second.Blob = blob;
						_dirtyPipelines.insert(_dirtyPipelines.end(), it->second.UsedByPipelines.begin(), it->second.UsedByPipelines.end());
						if (it->second.RootSignature) {
							_dirtyRootSignatures.push_back(it->second.RootSignature);
						}
						_mutex.unlock();
					}
					else {
						_mutex.unlock();
					}

					lastChangedPath = changedPath;
					lastChangedPathTimeStamp = changedPathWriteTime;
				}
			}

			offset += fileNotify->NextEntryOffset;
		} while (fileNotify->NextEntryOffset != 0);

		if (not ResetEvent(overlapped.hEvent)) {
			fprintf(stderr, "Reset event failed.\n");
		}

		DWORD error = ReadDirectoryChangesW(directoryHandle,
			buffer, sizeof(buffer), TRUE,
			eventName, NULL, &overlapped, NULL);

		if (error == 0) {
			fprintf(stderr, "Read directory failed.\n");
		}
	}

	return 0;
}

void DxRootSignature::CopyRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC *desc) {
	uint32 numDescriptorTables = 0;
	for (uint32 i = 0; i < desc->NumParameters; i++) {
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			numDescriptorTables++;
			SetBit(_tableRootParameterMask, i);
		}
	}

	_descriptorTableSizes = new uint32[numDescriptorTables];
	_numDescriptorTables = numDescriptorTables;

	uint32 index = 0;
	for (uint32 i = 0; i < desc->NumParameters; i++) {
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			uint32 numRanges = desc->pParameters[i].DescriptorTable.NumDescriptorRanges;
			_descriptorTableSizes[index] = 0;
			for (uint32 r = 0; r < numRanges; r++) {
				_descriptorTableSizes[index] += desc->pParameters[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
			}
			index++;
		}
	}
}

void DxRootSignature::CopyRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1 *desc) {
	uint32 numDescriptorTables = 0;
	for (uint32 i = 0; i < desc->NumParameters; i++) {
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			numDescriptorTables++;
			SetBit(_tableRootParameterMask, i);
		}
	}

	_descriptorTableSizes = new uint32[numDescriptorTables];
	_numDescriptorTables = numDescriptorTables;

	uint32 index = 0;
	for (uint32 i = 0; i < desc->NumParameters; i++) {
		if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
			uint32 numRanges = desc->pParameters[i].DescriptorTable.NumDescriptorRanges;
			_descriptorTableSizes[index] = 0;
			for (uint32 r = 0; r < numRanges; r++) {
				_descriptorTableSizes[index] += desc->pParameters[i].DescriptorTable.pDescriptorRanges[r].NumDescriptors;
			}
			index++;
		}
	}
}

void DxRootSignature::Initialize(DxBlob rootSignatureBlob) {
	DxContext& dxContext = DxContext::Instance();

	ThrowIfFailed(dxContext.GetDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));

	Com<ID3D12RootSignatureDeserializer> deserializer;
	ThrowIfFailed(D3D12CreateRootSignatureDeserializer(rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(deserializer.GetAddressOf())));
	D3D12_ROOT_SIGNATURE_DESC* desc = (D3D12_ROOT_SIGNATURE_DESC*)deserializer->GetRootSignatureDesc();

	CopyRootSignatureDesc(desc);
}

void DxRootSignature::Initialize(const wchar* path) {
	DxBlob rootSignatureBlob;
	ThrowIfFailed(D3DReadFileToBlob(path, rootSignatureBlob.GetAddressOf()));

	Initialize(rootSignatureBlob);
}

void DxRootSignature::Initialize(const D3D12_ROOT_SIGNATURE_DESC1& desc) {
	DxContext& dxContext = DxContext::Instance();
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(dxContext.GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(desc.NumParameters, desc.pParameters, desc.NumStaticSamplers, desc.pStaticSamplers, desc.Flags);

	DxBlob rootSignatureBlob;
	DxBlob errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

	ThrowIfFailed(dxContext.GetDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));

	CopyRootSignatureDesc(&desc);
}

void DxRootSignature::Initialize(CD3DX12_ROOT_PARAMETER1* rootParameters, uint32 numRootParameters,
	CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags) {
	D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = numRootParameters;
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = numSamplers;
	Initialize(rootSignatureDesc);
}

void DxRootSignature::Initialize(const D3D12_ROOT_SIGNATURE_DESC& desc) {
	DxContext& dxContext = DxContext::Instance();
	DxBlob rootSignatureBlob;
	DxBlob errorBlob;
	ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob));

	ThrowIfFailed(dxContext.GetDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));

	CopyRootSignatureDesc(&desc);
}

void DxRootSignature::Initialize(CD3DX12_ROOT_PARAMETER* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags) {
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = numRootParameters;
	rootSignatureDesc.pStaticSamplers = samplers;
	rootSignatureDesc.NumStaticSamplers = numSamplers;
	Initialize(rootSignatureDesc);
}

void DxRootSignature::Initialize(D3D12_ROOT_SIGNATURE_FLAGS flags) {
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = flags;
	Initialize(rootSignatureDesc);
}

DxCommandSignature CreateCommandSignature(DxRootSignature rootSignature, const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc) {
	DxContext& dxContext = DxContext::Instance();
	DxCommandSignature commandSignature;
	ThrowIfFailed(dxContext.GetDevice()->CreateCommandSignature(&commandSignatureDesc,
		commandSignatureDesc.NumArgumentDescs == 1 ? 0 : rootSignature.RootSignature(),
		IID_PPV_ARGS(commandSignature.GetAddressOf())));
	return commandSignature;
}

void DxRootSignature::Free() {
	if (_descriptorTableSizes) {
		delete[] _descriptorTableSizes;
	}
}


DxCommandSignature CreateCommandSignature(DxRootSignature rootSignature, D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs, uint32 numArgumentDescs, uint32 commandStructureSize) {
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc;
	commandSignatureDesc.pArgumentDescs = argumentDescs;
	commandSignatureDesc.NumArgumentDescs = numArgumentDescs;
	commandSignatureDesc.ByteStride = commandStructureSize;
	commandSignatureDesc.NodeMask = 0;

	return CreateCommandSignature(rootSignature, commandSignatureDesc);
}
