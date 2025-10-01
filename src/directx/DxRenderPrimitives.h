#pragma once

#include "dx.h"
#include "../core/threading.h"
#include "DxDescriptorHeap.h"
#include "DxContext.h"

#define UNBOUNDED_DESCRIPTOR_RANGE (-1)

DxRootSignature CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC1& desc);
DxRootSignature CreateRootSignature(CD3DX12_ROOT_PARAMETER1* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags);
DxRootSignature CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc);
DxRootSignature CreateRootSignature(CD3DX12_ROOT_PARAMETER* rootParameters, uint32 numRootParameters, CD3DX12_STATIC_SAMPLER_DESC* samplers, uint32 numSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags);
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