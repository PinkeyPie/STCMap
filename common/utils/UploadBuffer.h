#pragma once

#include "d3dUtil.h"

template<class T>
class UploadBuffer {
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : _bIsConstantBuffer(isConstantBuffer) {
		_nElementByteSize = sizeof(T);

		if (_bIsConstantBuffer) {
			_nElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));
		}

		// Constant buffer elements need to be multiples of 256 bytes.
		// This is because the hardware can only view constant data 
		// at m*256 byte offsets and of n*256 byte lengths. 
		// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
		// UINT64 OffsetInBytes; // multiple of 256
		// UINT   SizeInBytes;   // multiple of 256
		// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
		const CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(_nElementByteSize * elementCount);
		ThrowIfFailed(device->CreateCommittedResource(
			&properties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_pUploadBuffer)));

		ThrowIfFailed(_pUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_pMappedData)));
		// We do not need to unmap until we are done with the resource.  However, we must not write to
		// the resource while it is in use by the GPU (so we must use synchronization techniques).
	}

	UploadBuffer(const UploadBuffer& o) = delete;
	UploadBuffer& operator=(const UploadBuffer& o) = delete;
	~UploadBuffer() {
		if (_pUploadBuffer != nullptr) {
			_pUploadBuffer->Unmap(0, nullptr);
		}
		_pMappedData = nullptr;
	}

	[[nodiscard]] ID3D12Resource* Resource() const {
		return _pUploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data) {
		memcpy(&_pMappedData[elementIndex * _nElementByteSize], &data, sizeof(T));
	}
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> _pUploadBuffer;
	BYTE* _pMappedData = nullptr;

	UINT _nElementByteSize = 0;
	bool _bIsConstantBuffer = false;
};
