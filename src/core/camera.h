#pragma once

#include "math.h"
#include "../physics/colliders.h"

struct CameraIntrinsics {
	float Fx, Fy, Cx, Cy;
};

class CameraBase {
public:
	// Camera properties
	quat Rotation;
	vec3 Position;

	float NearPlane;
	float FarPlane = -1.f;

	// Derived values
	mat4 View;
	mat4 InvView;

	mat4 Proj;
	mat4 InvProj;

	mat4 ViewProj;
	mat4 InvViewProj;

	Ray GenerateWorldSpaceRay(float relX, float relY) const;
	Ray GenerateViewSpaceRay(float relX, float relY) const;
};

class RenderCamera : public CameraBase {
public:
	float VerticalFOV;

	void RecalculateMatrices(uint32 renderWidth, uint32 renderHeight);
	void RecalculateMatrices(float renderWidth, float renderHeight);
};

class RealCamera : public CameraBase {
public:
	CameraIntrinsics Intr;
	uint32 Width, Height;

	void RecalculateMatrices();
};