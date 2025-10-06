#include "DxRenderer.h"

DxRenderer* DxRenderer::_instance = new DxRenderer{};

void DxRenderer::Initialize(uint32 width, uint32 height) {
	RenderWidth = width;
	RenderHeight = height;

	DepthBuffer = DxTexture::CreateDepth(width, height, DXGI_FORMAT_D32_FLOAT);
}

void DxRenderer::BeginFrame(uint32 width, uint32 height, DxResource screenBackBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRtv) {
	if (RenderWidth != width and RenderHeight != height) {
		RenderWidth = width;
		RenderHeight = height;

		DepthBuffer.Resize(width, height);
	}
}

