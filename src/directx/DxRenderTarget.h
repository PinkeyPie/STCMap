#pragma once

#include "../pch.h"
#include "dx.h"
#include "DxTexture.h"

class DxRenderTarget {
public:
	DxResource ColorAttachments[8];
	DxResource DepthStencilAttachment;

	uint32 NumAttachments;
	uint32 Width;
	uint32 Height;
	D3D12_VIEWPORT Viewport;

	D3D12_RT_FORMAT_ARRAY RenderTargetFormat;
	DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_UNKNOWN;

	D3D12_CPU_DESCRIPTOR_HANDLE RtvHandles[8];
	D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle;

	uint32 PushColorAttachment(DxTexture& texture);
	void PushDepthStencilAttachment(DxTexture& texture);
	void NotifyOnTextureResize(uint32 width, uint32 height); // This doesn't resize the textures, only updates the width, height, and viewport variables
};