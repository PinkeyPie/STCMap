#pragma once

#include "../pch.h"
#include "dx.h"
#include "DxTexture.h"

class DxRenderTarget {
public:
	uint32 NumAttachments;
	D3D12_VIEWPORT Viewport;

	DxRtvDescriptorHandle RTV[9];
	DxDsvDescriptorHandle DSV;

	DxRenderTarget(std::initializer_list<Ptr<DxTexture>> colorAttachments, Ptr<DxTexture> depthAttachment = 0);
};