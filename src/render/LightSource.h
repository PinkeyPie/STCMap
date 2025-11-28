#pragma once

#include "../core/math.h"
#include "../core/camera.h"

#include "light_source.hlsli"

#define SHADOW_MAP_NEGATIVE_Z_OFFSET 1000.f

struct DirectionalLight {
    vec3 Direction;
    uint32 NumShadowCascades;

    vec3 Color;
    float Intensity;

    vec4 CascadeDistances;
    vec4 Bias;

    mat4 ViewProj[MAX_NUM_SHADOW_CASCADES];

    vec4 BlendDistances;
    uint32 ShadowDimensions;

    // 'PreventRotationalShimmering' uses bounding spheres instead of bounding boxes.
    // This prevents shimmering along shadow edges, when the camera rotates.
    // It slightly reduces shadow map resolution though.
    void UpdateMatrices(const RenderCamera& camera, bool preventRotationalShimmering = true);
};

mat4 GetSpotlightViewProjMatrix(const SpotLightCb& sl);