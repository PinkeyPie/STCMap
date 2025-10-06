#pragma once

#include "DxRenderPrimitives.h"

class DxRenderer {
public:
	static DxRenderer* Instance() {
		return _instance;
	}
	void Initialize(uint32 width, uint32 height);
	void BeginFrame(uint32 width, uint32 height, DxResource screenBackBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE screenRtv);

	DxTexture DepthBuffer;
	uint32 RenderWidth;
	uint32 RenderHeight;

	DxResource ScreenBackBuffer;
	CD3DX12_CPU_DESCRIPTOR_HANDLE ScreenRtv;
private:
	static DxRenderer* _instance;
};