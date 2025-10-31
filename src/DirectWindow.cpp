#include "DirectWindow.h"

#include "Application.h"
#include "directx/DxContext.h"

namespace {
	int32 ComputeIntersectionArea(int32 ax1, int32 ay1, int32 ax2, int32 ay2, int32 bx1, int32 by1, int32 bx2, int32 by2) {
		return max(0, min(ax2, bx2) - max(ax1, bx1)) * max(0, min(ay2, by2) - max(ay1, by1));
	}
}

void DirectWindow::CreateSwapchain(const DxCommandQueue& commandQueue) {
	DxFactory factory = DxContext::Instance().GetFactory();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = ClientWidth;
	swapChainDesc.Height = ClientHeight;
	if (_colorDepth == EColorDepth8) {
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else {
		assert(_colorDepth == EColorDepth10);
		swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	}
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1,0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = NUM_BUFFERED_FRAMES;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// It is recommended to always allow tearing if tearing support is available
	swapChainDesc.Flags = _tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	Com<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.NativeQueue.Get(),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		swapChain1.GetAddressOf()
	));

	ThrowIfFailed(factory->MakeWindowAssociation(hwnd, NULL));

	ThrowIfFailed(swapChain1.As(&_swapChain));
}

DXGI_FORMAT DirectWindow::GetBackBufferFormat() const {
	if (_colorDepth == EColorDepth8) {
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	return DXGI_FORMAT_R10G10B10A2_UNORM;
}

void DirectWindow::CheckForHdrSupport() {
	DxFactory factory = DxContext::Instance().GetFactory();
	RECT windowRect = { 0, 0, (LONG)ClientWidth, (LONG)ClientHeight };
	GetWindowRect(hwnd, &windowRect);

	if (_colorDepth == EColorDepth8) {
		_hdrSupport = false;
		return;
	}

	Com<IDXGIAdapter1> dxgiAdapter;
	ThrowIfFailed(factory->EnumAdapters1(0, &dxgiAdapter));

	uint32 i = 0;
	Com<IDXGIOutput> currentOutput;
	Com<IDXGIOutput> bestOutput;
	int32 bestIntersectArea = -1;

	while (dxgiAdapter->EnumOutputs(i, currentOutput.GetAddressOf()) != DXGI_ERROR_NOT_FOUND) {
		// Get the rectangle bounds of the app window
		int ax1 = windowRect.left;
		int ay1 = windowRect.top;
		int ax2 = windowRect.right;
		int ay2 = windowRect.bottom;

		// Get the rectangle bounds of current output
		DXGI_OUTPUT_DESC desc;
		ThrowIfFailed(currentOutput->GetDesc(&desc));
		RECT r = desc.DesktopCoordinates;
		int bx1 = r.left;
		int by1 = r.top;
		int bx2 = r.right;
		int by2 = r.bottom;

		// Compute the intersection
		int32 intersectArea = ComputeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
		if (intersectArea > bestIntersectArea) {
			bestOutput = currentOutput;
			bestIntersectArea = intersectArea;
		}

		++i;
	}

	Com<IDXGIOutput6> output6;
	ThrowIfFailed(bestOutput.As(&output6));

	DXGI_OUTPUT_DESC1 desc1;
	ThrowIfFailed(output6->GetDesc1(&desc1));

	_hdrSupport = desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
}

void DirectWindow::SetSwapChainColorSpace() const {
	// Rec2020 is the standard for UHD displays. The tonemap shader needs to apply the ST2084 curve before display.
	// Rec709 is the same as sRGB, just without the gamma curve. The tonemap shader needs to apply the gamma curve before display.
	DXGI_COLOR_SPACE_TYPE colorSpace = _hdrSupport and _colorDepth == EColorDepth10 ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	UINT colorSpaceSupport = 0;
	if (SUCCEEDED(_swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) and
		(colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) {
		ThrowIfFailed(_swapChain->SetColorSpace1(colorSpace));
	}

	if (not _hdrSupport) {
		ThrowIfFailed(_swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr));
		return;
	}

	struct DisplayChromaticities {
		float RedX;
		float RedY;
		float GreenX;
		float GreenY;
		float BlueX;
		float BlueY;
		float WhiteX;
		float WhiteY;
	};

	static const DisplayChromaticities chroma = {
		0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f // Display Gamut Rec2020
	};

	float maxOutputNits = 1000.f;
	float minOutputNits = 0.001f;
	float maxCLL = 2000.f;
	float maxFALL = 500.f;

	DXGI_HDR_METADATA_HDR10 hdr10MetaData = {};
	hdr10MetaData.RedPrimary[0] = (uint16)(chroma.RedX * 50000.f);
	hdr10MetaData.RedPrimary[1] = (uint16)(chroma.RedY * 50000.f);
	hdr10MetaData.GreenPrimary[0] = (uint16)(chroma.GreenX * 50000.f);
	hdr10MetaData.GreenPrimary[1] = (uint16)(chroma.GreenY * 50000.f);
	hdr10MetaData.BluePrimary[0] = (uint16)(chroma.BlueX * 50000.f);
	hdr10MetaData.BluePrimary[1] = (uint16)(chroma.BlueY * 50000.f);
	hdr10MetaData.WhitePoint[0] = (uint16)(chroma.WhiteX * 50000.f);
	hdr10MetaData.WhitePoint[1] = (uint16)(chroma.WhiteY * 50000.f);
	hdr10MetaData.MaxMasteringLuminance = (uint32)(maxOutputNits * 10000.f);
	hdr10MetaData.MinMasteringLuminance = (uint32)(minOutputNits * 10000.f);
	hdr10MetaData.MaxContentLightLevel = (uint16)(maxCLL);
	hdr10MetaData.MaxFrameAverageLightLevel = (uint16)(maxFALL);

	ThrowIfFailed(_swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &hdr10MetaData));
}

void DirectWindow::UpdateRenderTargetView() {
	ID3D12Device5* device = DxContext::Instance().GetDevice();

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < NUM_BUFFERED_FRAMES; i++) {
		DxResource backBuffer;
		ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(backBuffer.GetAddressOf())));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		_backBuffers[i] = backBuffer;

		SetName(_backBuffers[i], "BackBuffer");

		rtvHandle.Offset(_rtvDescriptorSize);
	}
}

bool DirectWindow::Initialize(const TCHAR* name, int initialWidth, int initialHeight) {
	bool result = BaseWindow::Initialize(name, initialWidth, initialHeight);
	if (!result) {
		return false;
	}

	DxContext& dxContext = DxContext::Instance();
	_tearingSupported = dxContext.CheckTearingSupport();

	CreateSwapchain(dxContext.RenderQueue);
	_currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.NumDescriptors = NUM_BUFFERED_FRAMES;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	ThrowIfFailed(dxContext.GetDevice()->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(_rtvDescriptorHeap.GetAddressOf())));
	_rtvDescriptorSize = dxContext.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CheckForHdrSupport();
	SetSwapChainColorSpace();
	UpdateRenderTargetView();

	assert(_depthFormat == DXGI_FORMAT_UNKNOWN or IsDepthFormat(_depthFormat));
	if (_depthFormat != DXGI_FORMAT_UNKNOWN) {
		_depthBuffer = DxTexture::CreateDepth(ClientWidth, ClientHeight, _depthFormat);
	}

	_initialized = true;

	return true;
}

void DirectWindow::Shutdown() {
	_initialized = false;
}

void DirectWindow::ResizeHandle() {
	if (_initialized) {
		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		DxContext& dxContext = DxContext::Instance();
		dxContext.FlushApplication();

		for (auto& backBuffer : _backBuffers) {
			backBuffer.Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(_swapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(_swapChain->ResizeBuffers(NUM_BUFFERED_FRAMES, ClientWidth, ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		CheckForHdrSupport();
		SetSwapChainColorSpace();

		_currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetView();

		if (_depthBuffer.Resource) {
			_depthBuffer.Resize(ClientWidth, ClientHeight);
		}
	}
}

PCWCH DirectWindow::ClassName() const {
	return L"DirectWindow";
}

void DirectWindow::SwapBuffers() {
	if (_initialized) {
		uint32 syncInterval = _vSync ? 1 : 0;
		uint32 presentFlags = _tearingSupported and not _vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(_swapChain->Present(syncInterval, presentFlags));

		_currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();
	}
}

LRESULT DirectWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;

	switch (uMsg) {
		case WM_SIZE:
			if (_open) {
				ClientWidth = LOWORD(lParam);
				ClientHeight = HIWORD(lParam);
				if (not _sizing) {
					ResizeHandle();
				}
			}
			break;
		case WM_ENTERSIZEMOVE:
			_sizing = true;
			break;
		case WM_EXITSIZEMOVE:
			_sizing = false;
			ResizeHandle();
			break;
		case WM_CLOSE: {
			DestroyWindow(hwnd);
			break;
		}
		case WM_DESTROY: {
			if (_open) {
				_open = false;
				Application::Instance()->NumOpenWindows--;
				Shutdown();
			}
			break;
		}
		default: {
			result = DefWindowProcW(hwnd, uMsg, wParam, lParam);
			break;
		}
	}

	return result;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DirectWindow::Rtv() const {
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(_currentBackBufferIndex, _rtvDescriptorSize);
	return rtv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DirectWindow::Dsv() const {
	if (_depthBuffer.Resource) {
		return _depthBuffer.DSVHandle.CpuHandle;
	}
	return {};
}

void DirectWindow::ToggleVSync() {
	_vSync = !_vSync;
}
