#include "DxRenderTarget.h"
#include "DxRenderPrimitives.h"

uint32 DxRenderTarget::PushColorAttachment(DxTexture& texture) {
	assert(texture.Resource);

	uint32 attachmentPoint = NumAttachments++;

	ColorAttachments[attachmentPoint] = texture.Resource;
	RtvHandles[attachmentPoint] = texture.RTVHandles.CpuHandle;

	D3D12_RESOURCE_DESC desc = texture.Resource->GetDesc();

	if (Width == 0 or Height == 0) {
		Width = (uint32)desc.Width;
		Height = desc.Height;
		Viewport = { 0,0,(float)Width, (float)Height, 0.f, 1.f };
	}
	else {
		assert(Width == desc.Width and Height == desc.Height);
	}

	RenderTargetFormat.NumRenderTargets = 0;
	for (uint32 i = 0; i < std::size(ColorAttachments); i++) {
		DxResource tex = ColorAttachments[i];
		if (tex) {
			RenderTargetFormat.RTFormats[RenderTargetFormat.NumRenderTargets++] = tex->GetDesc().Format;
		}
	}

	return attachmentPoint;
}

void DxRenderTarget::PushDepthStencilAttachment(DxTexture& texture) {
	assert(texture.Resource);

	DepthStencilAttachment = texture.Resource;
	DsvHandle = texture.DSVHandle.CpuHandle;

	D3D12_RESOURCE_DESC desc = texture.Resource->GetDesc();

	if (Width == 0 or Height == 0) {
		Width = (uint32)desc.Width;
		Height = desc.Height;
		Viewport = { 0,0,(float)Width, (float)Height, 0.f, 1.f };
	}
	else {
		assert(Width == desc.Width and Height == desc.Height);
	}

	DepthStencilFormat = GetDepthFormatFromTypeless(desc.Format);
}

void DxRenderTarget::Resize(uint32 width, uint32 height) {
	Width = width;
	Height = height;
	Viewport = { 0,0,(float)Width, (float)Height, 0.f, 1.f };
}