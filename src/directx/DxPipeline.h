#pragma once

#include "dx.h"
#include "DxContext.h"

#include <unordered_map>
#include <set>
#include <deque>
#include <mutex>


#define CREATE_GRAPHICS_PIPELINE DxGraphicsPipelineGenerator()
#define CREATE_COMPUTE_PIPELINE DxComputePipelineGenerator()

class DxGraphicsPipelineGenerator {
public:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc;

	operator const D3D12_GRAPHICS_PIPELINE_STATE_DESC&() const {
		return Desc;
	}

	DxGraphicsPipelineGenerator() {
		Desc = {};
		Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		Desc.SampleDesc = {.Count = 1, .Quality = 0 };
		Desc.SampleMask = 0xffffffff;
	}

	DxPipelineState Create() {
		DxPipelineState result;
		ThrowIfFailed(DxContext::Instance().GetDevice()->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(result.GetAddressOf())));
		return result;
	}

	DxGraphicsPipelineGenerator& RootSignature(DxRootSignature rootSignature) {
		Desc.pRootSignature = rootSignature.Get();
		return *this;
	}

	DxGraphicsPipelineGenerator& Vs(DxBlob shader) {
		Desc.VS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	DxGraphicsPipelineGenerator& Ps(DxBlob shader) {
		Desc.PS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	DxGraphicsPipelineGenerator& Gs(DxBlob shader) {
		Desc.GS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	DxGraphicsPipelineGenerator& Ds(DxBlob shader) {
		Desc.DS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	DxGraphicsPipelineGenerator& Hs(DxBlob shader) {
		Desc.HS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}

	DxGraphicsPipelineGenerator& AlphaToCoverage(bool enable) {
		Desc.BlendState.AlphaToCoverageEnable = enable;
		return *this;
	}

	DxGraphicsPipelineGenerator& IndependentRenderTargetBlending(bool enable) {
		Desc.BlendState.IndependentBlendEnable = enable;
		return *this;
	}

	DxGraphicsPipelineGenerator& BlendState(uint32 renderTargetIndex, D3D12_BLEND srcBlend, D3D12_BLEND destBlend, D3D12_BLEND_OP op) {
		assert(not Desc.BlendState.RenderTarget[renderTargetIndex].LogicOpEnable);
		Desc.BlendState.RenderTarget[renderTargetIndex].BlendEnable = true;
		Desc.BlendState.RenderTarget[renderTargetIndex].SrcBlend = srcBlend;
		Desc.BlendState.RenderTarget[renderTargetIndex].DestBlend = destBlend;
		Desc.BlendState.RenderTarget[renderTargetIndex].DestBlend = destBlend;
		Desc.BlendState.RenderTarget[renderTargetIndex].BlendOp = op;
		return *this;
	}

	DxGraphicsPipelineGenerator& AlphaBlending(uint32 renderTargetIndex) {
		return BlendState(renderTargetIndex, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD);
	}

	DxGraphicsPipelineGenerator& AdditiveBlending(uint32 renderTargetIndex) {
		return BlendState(renderTargetIndex, D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD);
	}

	DxGraphicsPipelineGenerator& Wireframe() {
		Desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		return *this;
	}

	DxGraphicsPipelineGenerator& CullFrontFaces() {
		Desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		return *this;
	}

	DxGraphicsPipelineGenerator& CullingOff() {
		Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return *this;
	}

	DxGraphicsPipelineGenerator& RasterizeCounterClockwise() {
		Desc.RasterizerState.FrontCounterClockwise = true;
		return *this;
	}

	DxGraphicsPipelineGenerator& DepthBias(int bias = D3D12_DEFAULT_DEPTH_BIAS, float biasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP, float slopeScaledBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS) {
		Desc.RasterizerState.DepthBias = bias;
		Desc.RasterizerState.DepthBiasClamp = biasClamp;
		Desc.RasterizerState.SlopeScaledDepthBias = slopeScaledBias;
		return *this;
	}

	DxGraphicsPipelineGenerator& Antialiasing(bool multisampling = false, bool antialiasedLines = false, uint32 forcedSampleCount = 0) {
		Desc.RasterizerState.MultisampleEnable = multisampling;
		Desc.RasterizerState.AntialiasedLineEnable = antialiasedLines;
		Desc.RasterizerState.ForcedSampleCount = forcedSampleCount;
		return *this;
	}

	DxGraphicsPipelineGenerator& RasterizeConservative() {
		Desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
		return *this;
	}

	DxGraphicsPipelineGenerator& DepthSettings(bool depthTest = true, bool depthWrite = true, D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_LESS) {
		Desc.DepthStencilState.DepthEnable = depthTest;
		Desc.DepthStencilState.DepthWriteMask = depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		Desc.DepthStencilState.DepthFunc = func;
		return *this;
	}

	DxGraphicsPipelineGenerator& StencilSettings(bool stencilTest = true) {
		Desc.DepthStencilState.StencilEnable = stencilTest;
		return *this;
	}

	template<uint32 numElements>
	DxGraphicsPipelineGenerator& InputLayout(D3D12_INPUT_ELEMENT_DESC (&elements)[numElements]) {
		Desc.InputLayout = {elements, numElements };
		return *this;
	}

	DxGraphicsPipelineGenerator& InputLayout(D3D12_INPUT_ELEMENT_DESC* elements, uint32 numElements) {
		Desc.InputLayout = { elements, numElements };
		return *this;
	}

	DxGraphicsPipelineGenerator& PrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
		Desc.PrimitiveTopologyType = type;
		return *this;
	}

	DxGraphicsPipelineGenerator& RenderTargets(DXGI_FORMAT* rtFormats, uint32 numRenderTargets, DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN) {
		memcpy(Desc.RTVFormats, rtFormats, sizeof(DXGI_FORMAT) * numRenderTargets);
		Desc.NumRenderTargets = numRenderTargets;
		Desc.DSVFormat = dsvFormat;
		return *this;
	}

	DxGraphicsPipelineGenerator& RenderTargets(D3D12_RT_FORMAT_ARRAY& rtFormats, DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN) {
		return RenderTargets(rtFormats.RTFormats, rtFormats.NumRenderTargets, dsvFormat);
	}

	DxGraphicsPipelineGenerator& Multisampling(uint32 count = 1, uint32 quality = 0) {
		Desc.SampleDesc = { count, quality };
		return *this;
	}
};

class DxComputePipelineGenerator {
public:
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc;

	operator const D3D12_COMPUTE_PIPELINE_STATE_DESC() const {
		return Desc;
	}

	DxComputePipelineGenerator() {
		Desc = {};
	}

	DxPipelineState Create() {
		DxPipelineState result;
		ThrowIfFailed(DxContext::Instance().GetDevice()->CreateComputePipelineState(&Desc, IID_PPV_ARGS(result.GetAddressOf())));
		return result;
	}

	DxComputePipelineGenerator& RootSignature(DxRootSignature rootSignature) {
		Desc.pRootSignature = rootSignature.Get();
		return *this;
	}

	DxComputePipelineGenerator& Cs(DxBlob shader) {
		Desc.CS = CD3DX12_SHADER_BYTECODE(shader.Get());
		return *this;
	}
};

class DxPipeline {
public:
	DxPipelineState* Pipeline;
	DxRootSignature* RootSignature;
};

struct GraphicsPipelineFiles {
	const char* Vs = nullptr;
	const char* Ps = nullptr;
	const char* Ds = nullptr;
	const char* Hs = nullptr;
	const char* Gs = nullptr;
};

class DxPipelineFactory {
public:
	DxPipelineFactory() = default;

	static DxPipelineFactory* Instance() {
		return _instance;
	}

	void CreateAllReloadablePipelines();
	void CheckForChangedPipelines();
	DxPipeline CreateReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const GraphicsPipelineFiles& files,
		DxRootSignature userRootSignature = nullptr);
	DxPipeline CreateReloadablePipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const GraphicsPipelineFiles& files,
		const char* rootSignatureFile);
private:
	struct ReloadablePipelineState {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};
		GraphicsPipelineFiles Files = {};

		DxPipelineState Pipeline = nullptr;
		DxRootSignature* RootSignature = nullptr;
		bool UserRootSignature = false;

		D3D12_INPUT_ELEMENT_DESC InputLayout[16] = {};

		ReloadablePipelineState() = default;

		void Initialize(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, const GraphicsPipelineFiles& files, DxRootSignature* rootSignature);
	};

	struct ReloadableRootSignature {
		const char* File;
		DxRootSignature RootSignature;
	};

	struct ShaderFile {
		DxBlob Blob;
		std::set<ReloadablePipelineState*> UsedByPipelines;

		ReloadableRootSignature* RootSignature;
	};

	ReloadableRootSignature* PushBlob(const char* filename, ReloadablePipelineState* pipelineIndex, bool isRootSignature = false);
	void LoadRootSignature(ReloadableRootSignature& r);
	void LoadPipeline(ReloadablePipelineState& p);
	DWORD CheckForFileChanges();

	static DxPipelineFactory* _instance;

	std::unordered_map<std::string, ShaderFile> _shaderBlobs = {};
	std::deque<ReloadablePipelineState> _pipelines = {};
	std::deque<ReloadableRootSignature> _rootSignatureFromFiles;
	std::deque<DxRootSignature> _userRootSignatures;

	std::vector<ReloadablePipelineState*> _dirtyPipelines = {};
	std::vector<ReloadableRootSignature*> _dirtyRootSignatures = {};

	std::mutex _mutex = {};

	friend void LoadRootSignature(ReloadableRootSignature& r);
};