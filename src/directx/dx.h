#pragma once

#include "dx/d3dx12.h"
#include <dxgi1_6.h>
#include <wrl.h>
#include <comdef.h>

template<class T>
using Com = Microsoft::WRL::ComPtr<T>;

class DxException : public std::exception {
public:
    DxException() = default;
    DxException(HRESULT hr, const std::string& funcName, const std::string& filename, int lineNumber)
	: ErrorCode(hr), FunctionName(funcName), FileName(filename), LineNumber(lineNumber) {}

    [[nodiscard]] std::string ToString() const {
        const _com_error err(ErrorCode);
        std::string fileInfo = __FILE__;
        const std::string msg(err.ErrorMessage());
        return FunctionName + " failed in " + FileName + "; line " + std::to_string(LineNumber) + "; error " + msg.c_str();
    }
    HRESULT ErrorCode = S_OK;
    std::string FunctionName;
    std::string FileName;
    int LineNumber = -1;
};

#ifdef _UNICODE
#ifndef ThrowIfFailed
#define ThrowIfFailed(hr)                                            \
{                                                                    \
HRESULT hr__ = (hr);                                                 \
std::wstring wfn = AnsiToWString(__FILE__);                          \
if(FAILED(hr__)) { throw DxException(hr__, L#hr, wfn, __LINE__); }   \
}
#endif
#else
#define ThrowIfFailed(hr)                                            \
{                                                                    \
HRESULT hr__ = (hr);                                                 \
std::string wfn = __FILE__;                                          \
if(FAILED(hr__)) { throw DxException(hr__, #hr, wfn, __LINE__); }    \
}
#endif

using DxObject = Com<ID3D12Object>;
using DxAdapter = Com<IDXGIAdapter4>;
using DxDevice = Com<ID3D12Device5>;
using DxFactory = Com<IDXGIFactory4>;
using DxSwapChain = Com<IDXGISwapChain4>;
using DxResource = Com<ID3D12Resource>;
using DxGraphicsCommandList = Com<ID3D12GraphicsCommandList4>;
using DxBlob = Com<ID3DBlob>;
using DxRootSignature = Com<ID3D12RootSignature>;
using DxPipelineState = Com<ID3D12PipelineState>;
using DxCommandSignature = Com<ID3D12CommandSignature>;
using DxHeap = Com<ID3D12Heap>;
using DxRaytracingPipelineState = Com<ID3D12StateObject>;

#define NUM_BUFFERED_FRAMES 2

#define SetName(obj, name) ThrowIfFailed(obj->SetName(L##name))

enum ColorDepth {
	EColorDepth8,
	EColorDepth10
};
