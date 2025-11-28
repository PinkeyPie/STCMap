#pragma once

#include "camera.h"
#include "input.h"

class CameraController
{
public:
    void Initialize(RenderCamera* camera) { Camera = camera; }

    // Returns true, if camera is moved, and therefore input is captured.
    void CenterCameraOnObject(const BoundingBox& aabb);
    bool Update(const UserInput& input, uint32 viewportWidth, uint32 viewportHeight, float dt);

    RenderCamera* Camera;

private:
    float _orbitRadius = 10.f;


    float _centeringTime = -1.f;

    vec3 _centeringPositionStart;
    quat _centeringRotationStart;

    vec3 _centeringPositionTarget;
    quat _centeringRotationTarget;
};