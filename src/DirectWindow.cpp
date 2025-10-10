#include "DirectWindow.h"

#include "Application.h"
#include "directx/DxContext.h"

namespace {
	int32 ComputeIntersectionArea(int32 ax1, int32 ay1, int32 ax2, int32 ay2, int32 bx1, int32 by1, int32 bx2, int32 by2) {
		return max(0, min(ax2, bx2) - max(ax1, bx1)) * max(0, min(ay2, by2) - max(ay1, by1));
	}
}

void DirectWindow::CheckTearingSupport() {
	DxFactory factory = DxContext::Instance().Factory;
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	Com<IDXGIFactory5> factory5;
	if (SUCCEEDED(factory.As(&factory5))) {
		if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
			allowTearing = FALSE;
		}
	}

	TearingSupported = allowTearing == TRUE;
}

void DirectWindow::CreateSwapchain(const DxCommandQueue& commandQueue) {
	DxFactory factory = DxContext::Instance().Factory;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = ClientWidth;
	swapChainDesc.Height = ClientHeight;
	if (ColorDepth == EColorDepth8) {
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	else {
		assert(ColorDepth == EColorDepth10);
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
	swapChainDesc.Flags = TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	Com<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		commandQueue.CommandQueue.Get(),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		swapChain1.GetAddressOf()
	));

	ThrowIfFailed(factory->MakeWindowAssociation(hwnd, NULL));

	ThrowIfFailed(swapChain1.As(&SwapChain));
}

DXGI_FORMAT DirectWindow::GetBackBufferFormat() const {
	if (ColorDepth == EColorDepth8) {
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	return DXGI_FORMAT_R10G10B10A2_UNORM;
}

void DirectWindow::CheckForHdrSupport() {
	DxFactory factory = DxContext::Instance().Factory;
	RECT windowRect = { 0, 0, (LONG)ClientWidth, (LONG)ClientHeight };
	GetWindowRect(hwnd, &windowRect);

	if (ColorDepth == EColorDepth8) {
		HdrSupport = false;
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

	HdrSupport = desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
}

void DirectWindow::SetSwapChainColorSpace() {
	// Rec2020 is the standard for UHD displays. The tonemap shader needs to apply the ST2084 curve before display.
	// Rec709 is the same as sRGB, just without the gamma curve. The tonemap shader needs to apply the gamma curve before display.
	DXGI_COLOR_SPACE_TYPE colorSpace = HdrSupport and ColorDepth == EColorDepth10 ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	UINT colorSpaceSupport = 0;
	if (SUCCEEDED(SwapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) and
		(colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) {
		ThrowIfFailed(SwapChain->SetColorSpace1(colorSpace));
	}

	if (not HdrSupport) {
		ThrowIfFailed(SwapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr));
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

	ThrowIfFailed(SwapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &hdr10MetaData));
}

void DirectWindow::UpdateRenderTargetView() {
	DxDevice device = DxContext::Instance().Device;
	uint32 rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(RtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < NUM_BUFFERED_FRAMES; i++) {
		DxResource backBuffer;
		ThrowIfFailed(SwapChain->GetBuffer(i, IID_PPV_ARGS(backBuffer.GetAddressOf())));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		BackBuffers[i] = backBuffer;

		SetName(BackBuffers[i], "BackBuffer");

		rtvHandle.Offset(RtvDescriptorSize);
	}
}

bool DirectWindow::Initialize(const TCHAR* name, uint32 initialWidth, uint32 initialHeight) {
	bool result = BaseWindow::Initialize(name, initialWidth, initialHeight);
	if (!result) {
		return false;
	}

	DxContext& dxContext = DxContext::Instance();
	CheckTearingSupport();

	CreateSwapchain(dxContext.RenderQueue);
	CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.NumDescriptors = NUM_BUFFERED_FRAMES;
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	ThrowIfFailed(dxContext.Device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(RtvDescriptorHeap.GetAddressOf())));
	RtvDescriptorSize = dxContext.Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CheckForHdrSupport();
	SetSwapChainColorSpace();
	UpdateRenderTargetView();

	assert(DepthFormat == DXGI_FORMAT_UNKNOWN or IsDepthFormat(DepthFormat));
	if (DepthFormat != DXGI_FORMAT_UNKNOWN) {
		DepthBuffer = DxTexture::CreateDepth(ClientWidth, ClientHeight, DepthFormat);
	}

	Initialized = true;

	return true;
}

void DirectWindow::Shutdown() {
	Initialized = false;
}

void DirectWindow::ResizeHandle() {
	if (Initialized) {
		// Flush the GPU queue to make sure the swap chain's back buffers
		// are not being referenced by an in-flight command list.
		DxContext& dxContext = DxContext::Instance();
		dxContext.FlushApplication();

		for (uint32 i = 0; i < NUM_BUFFERED_FRAMES; i++) {
			BackBuffers[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowIfFailed(SwapChain->GetDesc(&swapChainDesc));
		ThrowIfFailed(SwapChain->ResizeBuffers(NUM_BUFFERED_FRAMES, ClientWidth, ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		CheckForHdrSupport();
		SetSwapChainColorSpace();

		CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetView();

		if (DepthBuffer.Resource) {
			DepthBuffer.Resize(ClientWidth, ClientHeight);
		}
	}
}

PCWCH DirectWindow::ClassName() const {
	return L"DirectWindow";
}

void DirectWindow::SwapBuffers() {
	if (Initialized) {
		uint32 syncInterval = VSync ? 1 : 0;
		uint32 presentFlags = TearingSupported and not VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
		ThrowIfFailed(SwapChain->Present(syncInterval, presentFlags));

		CurrentBackBufferIndex = SwapChain->GetCurrentBackBufferIndex();
	}
}

LRESULT DirectWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;

	switch (uMsg) {
	case WM_SIZE:
		if (Open) {
			ClientWidth = LOWORD(lParam);
			ClientHeight = HIWORD(lParam);
			ResizeHandle();
		}
		break;
	case WM_CLOSE: {
		DestroyWindow(hwnd);
		break;
	}
	case WM_DESTROY: {
		if (Open) {
			Open = false;
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
