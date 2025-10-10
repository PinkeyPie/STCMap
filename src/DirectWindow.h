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

	bool Initialize(const TCHAR* name, uint32 initialWidth, uint32 initialHeight) override;

	virtual void Shutdown();
	virtual void SwapBuffers();
	virtual void ResizeHandle();
	
	PCTCH ClassName() const override;
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	DXGI_FORMAT GetBackBufferFormat() const;

	DxSwapChain SwapChain;
	DxResource BackBuffers[NUM_BUFFERED_FRAMES];
	Com<ID3D12DescriptorHeap> RtvDescriptorHeap;
	uint32 RtvDescriptorSize;
	uint32 CurrentBackBufferIndex;

	ColorDepth ColorDepth = EColorDepth8;

	RECT WindowRectBeforeFullscreen;

	bool TearingSupported = false;
	bool HdrSupport = false;
	bool VSync = false;
	bool Open = true;
	bool Initialized = false;

	DxTexture DepthBuffer;
	DXGI_FORMAT DepthFormat = DXGI_FORMAT_UNKNOWN;
private:
	void CreateSwapchain(const DxCommandQueue& commandQueue);
	void CheckForHdrSupport();
	void SetSwapChainColorSpace();
	void CheckTearingSupport();
	void UpdateRenderTargetView();
};
