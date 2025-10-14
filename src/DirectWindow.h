#pragma once

#include "BaseWindow.h"
#include "directx/dx.h"
#include "directx/DxCommandQueue.h"
#include "directx/DxRenderPrimitives.h"

class DirectWindow : public BaseWindow {
public:
	DirectWindow() = default;
	DirectWindow(const DirectWindow&) = delete;
	DirectWindow& operator=(const DirectWindow&) = delete;
	~DirectWindow() override = default;

	bool Initialize(const TCHAR* name, int initialWidth, int initialHeight) override;

	virtual void Shutdown();
	virtual void SwapBuffers();
	virtual void ResizeHandle();
	
	PCTCH ClassName() const override;
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	uint32 CurrentBackBufferIndex() const {
		return _currentBackBufferIndex;
	}
	DXGI_FORMAT GetBackBufferFormat() const;
	DxResource GetCurrentBackBuffer() const {
		return _backBuffers[_currentBackBufferIndex];
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv() const;
	uint32 RtvDescriptorSize() const {
		return _rtvDescriptorSize;
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;
	bool IsOpen() const {
		return _open;
	}
		
private:
	DxSwapChain _swapChain;
	DxResource _backBuffers[NUM_BUFFERED_FRAMES];
	ColorDepth _colorDepth = EColorDepth8;
	Com<ID3D12DescriptorHeap> _rtvDescriptorHeap;
	uint32 _currentBackBufferIndex;
	int32 _rtvDescriptorSize;

	DxTexture _depthBuffer;
	DXGI_FORMAT _depthFormat = DXGI_FORMAT_UNKNOWN;

	bool _tearingSupported = false;
	bool _hdrSupport = false;
	bool _vSync = false;
	bool _open = true;
	bool _initialized = false;

	void CreateSwapchain(const DxCommandQueue& commandQueue);
	void CheckForHdrSupport();
	void SetSwapChainColorSpace() const;
	void CheckTearingSupport();
	void UpdateRenderTargetView();
};
