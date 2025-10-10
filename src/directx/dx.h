#pragma once

#include "dx/d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <comdef.h>

template<class T>
using Com = Microsoft::WRL::ComPtr<T>;

inline std::wstring AnsiToWString(const std::string& str) {
    WCHAR buffer[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return { buffer };
}

class DxException : public std::exception {
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& funcName, const std::wstring& filename, int lineNumber)
	: ErrorCode(hr), FunctionName(funcName), FileName(filename), LineNumber(lineNumber) {}

    [[nodiscard]] std::wstring ToString() const {
        const _com_error err(ErrorCode);        
        std::wstring fileInfo = AnsiToWString(__FILE__);
        const std::wstring msg(err.ErrorMessage());
        return FunctionName + L" failed in " + FileName + L"; line " + std::to_wstring(LineNumber) + L"; error " + msg.c_str();
    }
    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring FileName;
    int LineNumber = -1;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(hr)                                            \
{                                                                    \
HRESULT hr__ = (hr);                                                 \
std::wstring wfn = AnsiToWString(__FILE__);                          \
if(FAILED(hr__))  throw DxException(hr__, L#hr, wfn, __LINE__);      \
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

static DXGI_FORMAT GetScreenFormat(ColorDepth colorDepth) {
    return (colorDepth == EColorDepth8) ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R10G10B10A2_UNORM;
}