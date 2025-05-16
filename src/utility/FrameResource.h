#pragma once

#include "utils/d3dUtil.h"
#include "utils/MathHelper.h"
#include "utils/UploadBuffer.h"

struct ObjectConstants {
	DirectX::XMFLOAT4X4 world = MathHelper::Identity4x4();
};

struct PassConstants {
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.f, 0.f, 0.f };
	float cbPerObjectPad1 = 0.f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.f, 0.f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.f, 0.f };
	float NearZ = 0.f;
	float FarZ = 0.f;
	float TotalTime = 0.f;
	float DeltaTime = 0.f;
};

struct Vertex {
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Color;
};

// Stores the resources for the CPU to build the comand lists for a frame
struct FrameResource {
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
	FrameResource(const FrameResource& other) = delete;
	FrameResource& operator=(const FrameResource& other) = delete;
	~FrameResource();

	// We cannot reset the allocator until the GPU is done processing the commands
	// So each frame needs their own allocator
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// We cannot update a cbuffer until the GPU is done processing the commands
	// that reference it. So each frame needs their own cbuffers
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	// Fence value to mark commands up to this fence point. This lets us
	// check if these frames resources are still in use by the GPU
	UINT Fence = 0;
};