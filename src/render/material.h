#pragma once

#include "LightSource.h"
#include "../directx/DxBuffer.h"
#include "../directx/DxTexture.h"

class DxCommandList;

struct CommonMaterialInfo {
    Ptr<DxTexture> Sky;
    Ptr<DxTexture> Irradiance;
    Ptr<DxTexture> Environment;
    Ptr<DxTexture> Brdf;

    Ptr<DxTexture> TiledCullingGrid;
    Ptr<DxBuffer> TiledObjectsIndexList;
    Ptr<DxBuffer> TiledSpotlightIndexList;
    Ptr<DxBuffer> PointLightBuffer;
    Ptr<DxBuffer> SpotlightBuffer;
    Ptr<DxBuffer> DecalBuffer;
    Ptr<DxBuffer> PointLightShadowInfoBuffer;
    Ptr<DxBuffer> SpotlightShadowInfoBuffer;

    Ptr<DxTexture> DecalTextureAtlas;

    Ptr<DxTexture> ShadowMap;

    Ptr<DxTexture> VolumetricsTexture;

    Ptr<DxTexture> OpaqueDepth;
    Ptr<DxTexture> WorldNormals;

    DxDynamicConstantBuffer CameraCBV;
    DxDynamicConstantBuffer SunCBV;

    float EnvironmentIntensity;
    float SkyIntensity;
};

using MaterialSetupFunction = void(*)(DxCommandList*, const CommonMaterialInfo&);

// All materials must conform to the following standards:
// - Have a static function setupOpaquePipeline and/or setupTransparentPipeline of type material_setup_function, which binds the shader and initializes common stuff.
// - Have a function void prepareForRendering, which sets up uniforms specific to this material instance.
// - Initialize the shader to the dx_renderer's HDR render target and have the depth test to EQUAL.
// - Currently all materials must use the default_vs vertex shader and have the transform bound to root parameter 0.

struct MaterialBase {
    virtual void PrepareForRendering(DxCommandList* commandList) = 0;
};