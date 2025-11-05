#pragma once

#include "dx.h"
#include "DxContext.h"

#define UNBOUNDED_DESCRIPTOR_RANGE (-1)

class DxRootSignature {
public:
    ID3D12RootSignature* RootSignature() const { return _rootSignature.Get(); }
    uint32 NumDescriptorTables() const { return _numDescriptorTables; }
    const uint32* DescriptorTableSize() const { return _descriptorTableSizes; }
    uint32 TableRootParameterMask() const { return _tableRootParameterMask; }

    void Free();

    static DxRootSignature CreateRootSignature(DxBlob rootSignatureBlob);
    static DxRootSignature CreateRootSignature(const wchar* path);
    static DxRootSignature CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& desc);
    static DxRootSignature CreateRootSignature(CD3DX12_ROOT_PARAMETER1* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags);
    static DxRootSignature CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);
    static DxRootSignature CreateRootSignature(CD3DX12_ROOT_PARAMETER* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags);
    static DxRootSignature CreateRootSignature(D3D12_ROOT_SIGNATURE_FLAGS flags);

private:
    void CopyRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC* desc);
    void CopyRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1* desc);

    Com<ID3D12RootSignature> _rootSignature = nullptr;
    uint32 _numDescriptorTables = 0;
    uint32* _descriptorTableSizes = nullptr;
    uint32 _tableRootParameterMask = 0;
};

DxCommandSignature CreateCommandSignature(DxRootSignature rootSignature, const D3D12_COMMAND_SIGNATURE_DESC& commandSignatureDesc);
DxCommandSignature CreateCommandSignature(DxRootSignature rootSignature, D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs, uint32 numArgumentDescs, uint32 commandStructureSize);

bool IsUAVCompatibleFormat(DXGI_FORMAT format);
bool IsSRGBFormat(DXGI_FORMAT format);
bool IsBGRFormat(DXGI_FORMAT format);
bool IsDepthFormat(DXGI_FORMAT format);
bool IsTypelessFormat(DXGI_FORMAT format);
DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT format);
DXGI_FORMAT GetUAVCompatibleFormat(DXGI_FORMAT format);
DXGI_FORMAT GetDepthFormatFromTypeless(DXGI_FORMAT format);
DXGI_FORMAT GetReadFormatFromTypeless(DXGI_FORMAT format);
uint32 GetFormatSize(DXGI_FORMAT format);
bool CheckFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT1 support);
bool CheckFormatSupport(D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport, D3D12_FORMAT_SUPPORT2 support);

DxDevice GetDevice(Com<ID3D12DeviceChild> object);