#pragma once

#include "BaseWindow.h"
#include "directx/dx.h"
#include "directx/DxCommandQueue.h"
#include "directx/DxDescriptor.h"

class DirectWindow : public BaseWindow {
public:
	DirectWindow() = default;
	DirectWindow(const DirectWindow&) = delete;
	DirectWindow& operator=(const DirectWindow&) = delete;
	~DirectWindow() override;

	bool Initialize(const TCHAR* name, int initialWidth, int initialHeight, ColorDepth colorDepth) override;

	virtual void Shutdown();
	virtual void SwapBuffers();
	virtual void ResizeHandle();
	void ToggleVSync();
	
	PCTCH ClassName() const override;
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	uint32 CurrentBackBufferIndex() const {
		return _currentBackBufferIndex;
	}
	DXGI_FORMAT GetBackBufferFormat() const;
	DxResource GetCurrentBackBuffer() const {
		return _backBuffers[_currentBackBufferIndex];
	}
	DxRtvDescriptorHandle Rtv() const;
	uint32 RtvDescriptorSize() const {
		return _rtvDescriptorSize;
	}
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

	bool _tearingSupported = false;
	bool _hdrSupport = false;
	bool _vSync = false;
	bool _open = true;
	bool _initialized = false;
	bool _sizing = false;

	void CreateSwapchain(const DxCommandQueue& commandQueue);
	void CheckForHdrSupport();
	void SetSwapChainColorSpace() const;
	void CheckTearingSupport();
	void UpdateRenderTargetView();
};
