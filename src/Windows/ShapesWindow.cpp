#include "ShapesWindow.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int NumFrameResources = 3;

struct RenderItem {
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation
	// and scale of the object in the world
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource. Thus, when we modify object data we should set
	// NumFramesDirty = NumFrameResources so that each frame resource gets the update
	int NumFramesDirty = NumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item
	UINT ObjCBIndex = -1;

	MeshGeometry* Geometry = nullptr;

	// Primitive topology
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

ShapesWindow::~ShapesWindow() {
	if (_pDevice != nullptr) {
		FlushCommandQueue();
	}
}

bool ShapesWindow::Initialize() {
	if (not D3DApp::Initialize()) {
		return false;
	}
	// Reset the command list to prep for inititalization commands
	ThrowIfFailed(_pCommandList->Reset(_pDirectCommandListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
	BuildPSOs();

	// Execute the inititalization commands
	ThrowIfFailed(_pCommandList->Close());
	ID3D12CommandList* cmdLists[] = { _pCommandList.Get() };
	_pCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Wait until inititalization is complete
	FlushCommandQueue();

	return true;
}

void ShapesWindow::ResizeHandle() {
	D3DApp::ResizeHandle();

	// The window resized, so update the aspect ratio and recompute the projection matrix
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.f);
	XMStoreFloat4x4(&_proj, proj);
}

void ShapesWindow::Update(const GameTimer& gt) {
	KeyboardInputHandle(gt);
	UpdateCamera(gt);

	// Cycle through the circular frame resource array
	_nCurrFrameResourceIndex = (_nCurrFrameResourceIndex + 1) % NumFrameResources;
	_pCurrFrameResource = _frameResources[_nCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point
	if (_pCurrFrameResource->Fence != 0 and _pFence->GetCompletedValue() < _pCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(_pFence->SetEventOnCompletion(_pCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
}

void ShapesWindow::Draw(const GameTimer& gt) {
	auto cmdListAlloc = _pCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording
	// We can only reset when the associated command lists have finished execution on the GPU
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList
	// Reusing the command list reuses memory
	if (_bIsWireframe) {
		ThrowIfFailed(_pCommandList->Reset(cmdListAlloc.Get(), _psos["opaque_wireframe"].Get()));
	}
	else {
		ThrowIfFailed(_pCommandList->Reset(cmdListAlloc.Get(), _psos["opaque"].Get()));
	}

	_pCommandList->RSSetViewports(1, &_screenViewport);
	_pCommandList->RSSetScissorRects(1, &_scissorRect);

	// Indicate a state transition on the resource usage
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_pCommandList->ResourceBarrier(1, &barrier);

	// Clear the back buffer and depth buffer
	_pCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	_pCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferView = CurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = DepthStencilView();
	_pCommandList->OMSetRenderTargets(1, &backBufferView, true, &depthStencilView);

	ID3D12DescriptorHeap* descriptorHeaps[] = { _pCbvHeap.Get() };
	_pCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	_pCommandList->SetGraphicsRootSignature(_pRootSignature.Get());

	int passCbvIndex = _nPassCbvOffset + _nCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(_pCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, _nCbvSrvUavDescriptorSize);
	_pCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderItems(_pCommandList.Get(), _opaqueRItems);

	// Indicate a state transition on the resource usage
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	// Done recording commands
	ThrowIfFailed(_pCommandList->Close());

	// Add the command list to the queue for execution
	ID3D12CommandList* cmdLists[] = { _pCommandList.Get() };
	_pCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	// Advance the fence value to mark commands up to this fence point
	_pCurrFrameResource->Fence = ++_nCurrentFence;

	// Add an instruction to the command queue to set a new fenct point.
	// Because we are on the GPU timeline, the new fence point won't be
	// set until the GPU finishes processing all the commands prior to this Signal()
	_pCommandQueue->Signal(_pFence.Get(), _nCurrentFence);
}

void ShapesWindow::MouseDawnHandle(WPARAM btnState, int x, int y) {
	_lastMousePos.x = x;
	_lastMousePos.y = y;

	SetCapture(hwnd);
}

void ShapesWindow::MouseUpHandle(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void ShapesWindow::MouseMoveHandle(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - _lastMousePos.y));

		// Update angles based on input to orbit camera around box
		_fTheta += dx;
		_fPhi += dy;

		// Restrict the angle phi
		_fPhi = MathHelper::Clamp(_fPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to 0.2 unit in the scene
		float dx = 0.05f * static_cast<float>(x - _lastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - _lastMousePos.y);

		// Update the camera raduis based on input
		_fRadius += dx - dy;

		// Restrict the radius
		_fRadius = MathHelper::Clamp(_fRadius, 5.f, 150.f);
	}

	_lastMousePos.x = x;
	_lastMousePos.y = y;
}

void ShapesWindow::KeyboardInputHandle(const GameTimer& gt) {
	if (GetAsyncKeyState('1') & 0x8000) {
		_bIsWireframe = true;
	}
	else {
		_bIsWireframe = false;
	}
}

void ShapesWindow::UpdateCamera(const GameTimer& gt) {
	// Convert Spherical to Cartesian coordinates
	_eyePos.x = _fRadius * sinf(_fPhi) * cosf(_fTheta);
	_eyePos.z = _fRadius * sinf(_fPhi) * sinf(_fTheta);
	_eyePos.y = _fRadius * cosf(_fPhi);

	// Build the view matrix
	XMVECTOR pos = XMVectorSet(_eyePos.x, _eyePos.y, _eyePos.z, 1.f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
}

