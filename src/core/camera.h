#pragma once

#include "math.h"
#include "../physics/bounding_volumes.h"

union CameraFrustumCorners {
	vec3 eye;

	struct {
		vec3 NearTopLeft;
		vec3 NearTopRight;
		vec3 NearBottomLeft;
		vec3 NearBottomRight;
		vec3 FarTopLeft;
		vec3 FarTopRight;
		vec3 FarBottomLeft;
		vec3 FarBottomRight;
	};
	struct {
		vec3 Corners[8];
	};

	CameraFrustumCorners() {}
};

union CameraFrustumPlanes {
	struct {
		vec4 NearPlane;
		vec4 FarPlane;
		vec4 LeftPlane;
		vec4 RightPlane;
		vec4 TopPlane;
		vec4 BottomPlane;
	};
	vec4 planes[6];

	CameraFrustumPlanes() {}

	// Returns true, if object should be culled.
	bool CullWorldSpaceAABB(const BoundingBox& aabb) const;
	bool CullModelSpaceAABB(const BoundingBox& aabb, const trs& transform) const;
	bool CullModelSpaceAABB(const BoundingBox& aabb, const mat4& transform) const;
};

enum ECameraType {
	ECameraTypeIngame,
	ECameraTypeCalibrated
};

struct CameraProjectionExtents {
	float Left, Right, Top, Bottom; // Extents of frustum at distance 1.
};

class RenderCamera {
public:
	quat Rotation;
	vec3 Position;

	float NearPlane;
	float FarPlane = -1.f;

	uint32 Width, Height;
	ECameraType Type;

	union {
		float verticalFOV;

		struct {
			float FX, FY, CX, CY;
		};
	};

	// Derived values.
	mat4 View;
	mat4 InvView;

	mat4 Proj;
	mat4 InvProj;

	mat4 ViewProj;
	mat4 InvViewProj;

	float Aspect;

	void InitializeIngame(vec3 position, quat rotation, float verticalFOV, float nearPlane, float farPlane = -1.f);
	void InitializeCalibrated(vec3 position, quat rotation, uint32 width, uint32 height, float fx, float fy, float cx, float cy, float nearPlane, float farPlane = -1.f);

	void SetViewport(uint32 width, uint32 height);

	void UpdateMatrices();

	Ray GenerateWorldSpaceRay(float relX, float relY) const;
	Ray GenerateViewSpaceRay(float relX, float relY) const;

	vec3 RestoreViewSpacePosition(vec2 uv, float depthBufferDepth) const;
	vec3 RestoreWorldSpacePosition(vec2 uv, float depthBufferDepth) const;
	float DepthBufferDepthToEyeDepth(float depthBufferDepth) const;
	float EyeDepthToDepthBufferDepth(float eyeDepth) const;
	float LinearizeDepthBuffer(float depthBufferDepth) const;

	CameraFrustumCorners GetWorldSpaceFrustumCorners(float alternativeFarPlane = 0.f) const;
	CameraFrustumPlanes GetWorldSpaceFrustumPlanes() const;

	CameraFrustumCorners GetViewSpaceFrustumCorners(float alternativeFarPlane = 0.f) const;

	CameraProjectionExtents GetProjectionExtents() const;
	float GetMinProjectionExtent() const;

	RenderCamera GetJitteredVersion(vec2 offset) const;
};

CameraFrustumPlanes GetWorldSpaceFrustumPlanes(const mat4& viewProj);