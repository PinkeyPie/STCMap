#pragma once

#include "../core/math.h"
#include "../core/input.h"
#include "../core/camera.h"
#include "../directx/DxRenderer.h"

enum ETransformationType {
    ETransformationTypeNone = -1,
    ETransformationTypeTranslation,
    ETransformationTypeRotation,
    ETransformationTypeScale
};

enum ETransformationSpace {
    ETransformationGlobal,
    ETransformationLocal,
};

static const char* transformationTypeNames[] = {
    "Translation",
    "Rotation",
    "Scale",
};

static const char* transformationSpaceNames[] = {
    "Global",
    "Local",
};

void InitializeTransformationGizmos();
bool ManipulateTransformation(trs& transform, ETransformationType& type, ETransformationSpace& space, const RenderCamera& camera, const UserInput& input, bool allowInput, OverlayRenderPass* overlayRenderPass);

