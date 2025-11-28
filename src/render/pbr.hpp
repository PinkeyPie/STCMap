#pragma once

#include "../core/math.h"
#include "material.h"
#include "../directx/DxPipeline.h"

struct PbrEnvironment {
    Ptr<DxTexture> Sky;
    Ptr<DxTexture> Environment;
    Ptr<DxTexture> Irradiance;
};

class PbrMaterial : public MaterialBase {
public:
    PbrMaterial() = default;
    PbrMaterial(Ptr<DxTexture> albedo, Ptr<DxTexture> normal, Ptr<DxTexture> roughness, Ptr<DxTexture> metallic, const vec4& emission, const vec4& albedoTint, float roughnessOverride, float metallicOverride) :
    Albedo(albedo), Normal(normal), Roughness(roughness), Metallic(metallic), Emission(emission), AlbedoTint(albedoTint), RoughnessOverride(roughnessOverride), MetallicOverride(metallicOverride) {}

    Ptr<DxTexture> Albedo;
    Ptr<DxTexture> Normal;
    Ptr<DxTexture> Roughness;
    Ptr<DxTexture> Metallic;

    vec4 Emission;
    vec4 AlbedoTint;
    float RoughnessOverride;
    float MetallicOverride;

    void PrepareForRendering(DxCommandList *commandList) override;

    static void SetupOpaquePipeline(DxCommandList* commandList, const CommonMaterialInfo& info);
    static void SetupTransparentPipeline(DxCommandList* commandList, const CommonMaterialInfo& info);
    static void InitializePipeline();

private:
    static void SetupCommon(DxCommandList* commandList, const CommonMaterialInfo& info);
};

Ptr<PbrMaterial> CreatePBRMaterial(const char* albedoTex, const char* normalTex, const char* roughTex, const char* metallicTex, const vec4& emission = vec4(0.f), const vec4& albedoTint = vec4(1.f), float roughOverride = 1.f, float metallicOverride = 0.f);
Ptr<PbrMaterial> GetDefaultPBRMaterial();
Ptr<PbrEnvironment> CreateEnvironment(const char* filename, uint32 skyResolution = 2048, uint32 environmentResolution = 128, uint32 irradianceResolution = 32, bool asyncCompute = false);
