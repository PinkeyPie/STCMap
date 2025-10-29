#pragma once

#include "../core/math.h"
#include "../core/camera.h"

#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SHADOW_MAP_NEGATIVE_Z_OFFSET 100.f

struct LightAttenuation {
    float Constant = 1.f;
    float Linear;
    float Quadratic;

    float GetAttenuation(float distance) const {
        return 1.f / (Constant + Linear * distance + Quadratic * distance * distance);
    }

    float GetMaxDistance(float lightMax) const {
        return (-Linear + sqrt(Linear * Linear - 4.f * Quadratic * (Constant - (256.f / 5.f) * lightMax))) / (2.f * Quadratic);
    }
};

struct DirectionalLight {
    mat4 ViewProj[MAX_NUM_SUN_SHADOW_CASCADES];
    vec4 CascadeDistances;
    vec4 Bias;

    vec4 WorldSpaceDirection;
    vec4 Color;

    uint32 NumShadowCascades = 3;
    float BlendArea;
    float TexelSize;
    uint32 ShadowMapDimensions = 2048;

    void UpdateMatrices(const RenderCamera& camera);
};

struct SpotLight {
    mat4 ViewProj;

    vec4 WorldSpacePosition;
    vec4 WorldSpaceDirection;
    vec4 Color;

    LightAttenuation Attenuation;

    float InnerAngle;
    float OuterAngle;
    float InnerCutoff;  // cos(InnerAngle)
    float OuterCutoff;  // cos(OuterAngle)
    float TexelSize;
    float Bias;
    uint32 ShadowMapDimensions = 2048;

    void UpdateMatrices();
};

struct PointLight {
    vec4 WorldSpacePositionAndRadius;
    vec4 Color;
};