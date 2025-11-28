#pragma once

#include "../directx/DxPipeline.h"
#include "../directx/DxTexture.h"

class DxCommandList;

class TexturePreprocessor {
public:
    TexturePreprocessor() = default;

    void Initialize();

    void GenerateMipMapsOnGPU(DxCommandList* cl, Ptr<DxTexture> &texture);

    Ptr<DxTexture> EquirectangularToCubemap(DxCommandList* cl, const Ptr<DxTexture> &equirectangular, uint32 resolution,
                                            uint32 numMips = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

    Ptr<DxTexture> CubemapToIrradiance(DxCommandList *cl, const Ptr<DxTexture> &environment, uint32 resolution = 32,
                                       uint32 sourceSlice = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                                       float uvzScale = 1.f);

    Ptr<DxTexture> PrefilterEnvironment(DxCommandList* cl, const Ptr<DxTexture> &environment, uint32 resolution);

    Ptr<DxTexture> IntegrateBRDF(DxCommandList *cl, uint32 resolution = 512);

    static TexturePreprocessor* Instance() {
        return _instance;
    }

private:
    static TexturePreprocessor* _instance;

    DxPipeline _mipmapPipeline;
    DxPipeline _equirectangularToCubemapPipeline;
    DxPipeline _cubemapToIrradiancePipeline;
    DxPipeline _prefilteredEnvironmentPipeline;
    DxPipeline _integrateBRDFPipeline;
};