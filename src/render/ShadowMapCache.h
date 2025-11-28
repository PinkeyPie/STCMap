#pragma once

#include "../core/math.h"

struct ShadowMapViewport {
    int32 CpuVP[4]; // x, y, width, height.
    vec4 ShaderVP;
};

struct ShadowMapLightInfo {
    ShadowMapViewport Viewport; // Previous frame viewport. Gets updated by cache.
    bool LightMovedOrAppeared;
    bool GeometryInRangeMoved;
    //uint64 dynamicGeometryHash;
    //uint64 prevFrameDynamicGeometryHash;
    //uint64 lightTransformHash;
    //uint64 prevFrameLightTransformHash;

    static void TestShadowMapCache(ShadowMapLightInfo* infos, uint32 numInfos);
};
