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

#include "DxRenderPrimitives.h"

namespace fs = std::filesystem;

static const wchar* shaderDir = L"shaders//";

DxPipelineFactory* DxPipelineFactory::_instance = new DxPipelineFactory{};

static std::wstring StringToWideString(const std::string& s) {
	return std::wstring(s.begin(), s.end());
}

void DxPipelineFactory::ReloadablePipelineState::Initialize(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, const GraphicsPipelineFiles &files, DxRootSignature *rootSignature) {
	Type = EPipelineTypeGraphics;
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

void DxPipelineFactory::ReloadablePipelineState::Initialize(const char *file, DxRootSignature *rootSignature) {
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
				_rootSignatureFromFiles.push_back({filename, nullptr});
				result = &_rootSignatureFromFiles.back();
			}

			_shaderBlobs[filename] = {.Blob = blob, .UsedByPipelines = {pipelineIndex} };
		}
		else {
			// Already used
			it->second.UsedByPipelines.insert(pipelineIndex);

			if (isRootSignature) {
				if (!it->second.RootSignature) {
					_rootSignatureFromFiles.push_back({filename, nullptr});
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

	PushBlob(files.Vs, &state);
	PushBlob(files.Ps, &state);
	PushBlob(files.Gs, &state);
	PushBlob(files.Ds, &state);
	PushBlob(files.Hs, &state);

	_userRootSignatures.push_back(userRootSignature);
	DxRootSignature* rootSignature = &_userRootSignatures.back();
	_userRootSignatures.back() = userRootSignature; // Fuck you

	state.Initialize(desc, files, rootSignature);

	DxPipeline result = {&state.Pipeline, rootSignature};
	return result;
}

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc, const GraphicsPipelineFiles &files, const char *rootSignatureFile) {
	if (not rootSignatureFile) {
		rootSignatureFile = files.Ps;
	}

	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	ReloadableRootSignature* reloadableRs = PushBlob(rootSignatureFile, &state, true);
	PushBlob(files.Vs, &state);
	PushBlob(files.Ps, &state);
	PushBlob(files.Gs, &state);
	PushBlob(files.Ds, &state);
	PushBlob(files.Hs, &state);

	DxRootSignature* rootSignature = reloadableRs->RootSignature;

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

DxPipeline DxPipelineFactory::CreateReloadablePipeline(const char *csFile, const char *rootSignatureFile) {
	if (not rootSignatureFile) {
		rootSignatureFile = csFile;
	}

	_pipelines.emplace_back();
	auto& state = _pipelines.back();

	ReloadableRootSignature* reloadableRs = PushBlob(rootSignatureFile, &state, true);
	PushBlob(csFile, &state);

	DxRootSignature* rootSignature = reloadableRs->RootSignature;

	state.Initialize(csFile, rootSignature);

	DxPipeline result = {&state.Pipeline, rootSignature};
	return result;
}

void DxPipelineFactory::LoadRootSignature(ReloadableRootSignature &r) {
	DxBlob rs = _shaderBlobs[r.File].Blob;

	DxContext::Instance().RetireObject(r.RootSignature->RootSignature());
	r.RootSignature->Free();
	*r.RootSignature = DxRootSignature::CreateRootSignature(rs);
}

void DxPipelineFactory::LoadPipeline(ReloadablePipelineState& p) {
	DxContext& dxContext = DxContext::Instance();
	dxContext.RetireObject(p.Pipeline);

	if (p.Type == EPipelineTypeGraphics) {
		if (p.GraphicsFiles.Vs) {
			DxBlob shader = _shaderBlobs[p.GraphicsFiles.Vs].Blob;
			p.GraphicsDesc.VS = CD3DX12_SHADER_BYTECODE(shader.Get());
		}
		if (p.GraphicsFiles.Ps) {
			DxBlob shader = _shaderBlobs[p.GraphicsFiles.Ps].Blob;
			p.GraphicsDesc.PS = CD3DX12_SHADER_BYTECODE(shader.Get());
		}
		if (p.GraphicsFiles.Gs) {
			DxBlob shader = _shaderBlobs[p.GraphicsFiles.Gs].Blob;
			p.GraphicsDesc.GS = CD3DX12_SHADER_BYTECODE(shader.Get());
		}
		if (p.GraphicsFiles.Ds) {
			DxBlob shader = _shaderBlobs[p.GraphicsFiles.Ds].Blob;
			p.GraphicsDesc.DS = CD3DX12_SHADER_BYTECODE(shader.Get());
		}
		if (p.GraphicsFiles.Hs) {
			DxBlob shader = _shaderBlobs[p.GraphicsFiles.Hs].Blob;
			p.GraphicsDesc.HS = CD3DX12_SHADER_BYTECODE(shader.Get());
		}

		p.GraphicsDesc.pRootSignature = p.RootSignature->RootSignature();
		ThrowIfFailed(dxContext.GetDevice()->CreateGraphicsPipelineState(&p.GraphicsDesc, IID_PPV_ARGS(p.Pipeline.GetAddressOf())));
	}
	else {
		DxBlob shader = _shaderBlobs[p.ComputeFile].Blob;
		p.ComputeDesc.CS = CD3DX12_SHADER_BYTECODE(shader.Get());

		p.ComputeDesc.pRootSignature = p.RootSignature->RootSignature();
		ThrowIfFailed(dxContext.GetDevice()->CreateComputePipelineState(&p.ComputeDesc, IID_PPV_ARGS(p.Pipeline.GetAddressOf())));
	}
}

void DxPipelineFactory::CreateAllReloadablePipelines() {
	concurrency::parallel_for(0, (int)_rootSignatureFromFiles.size(), [&](int i) {
		LoadRootSignature(_rootSignatureFromFiles[i]);
	});

	concurrency::parallel_for(0, (int)_pipelines.size(), [&](int i) {
		LoadPipeline(_pipelines[i]);
	});

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
					auto it = _shaderBlobs.find(changedPath.stem().string());
					if (it != _shaderBlobs.end()) {
						while (FileIsLocked(changedPath.wstring().c_str())) {}

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
