#include "LightSource.h"

void DirectionalLight::UpdateMatrices(const RenderCamera &camera, bool preventRotationalShimmering) {
    mat4 viewMatrix = LookAt(vec3(0.f, 0.f, 0.f), Direction, vec3(0.f, 1.f, 0.f));

    CameraFrustumCorners viewFrustum = camera.GetViewSpaceFrustumCorners(CascadeDistances.w);

    // View space
    vec4 viewBottomLeft(viewFrustum.FarBottomLeft - viewFrustum.NearBottomLeft, 0.f);
    vec4 viewBottomRight(viewFrustum.FarBottomRight - viewFrustum.NearBottomRight, 0.f);
	vec4 viewTopLeft(viewFrustum.FarTopLeft - viewFrustum.NearTopLeft, 0.f);
	vec4 viewTopRight(viewFrustum.FarTopRight - viewFrustum.NearTopRight, 0.f);

	// Normalize to z == -1.
	viewBottomLeft /= -viewBottomLeft.z;
	viewBottomRight /= -viewBottomRight.z;
	viewTopLeft /= -viewTopLeft.z;
	viewTopRight /= -viewTopRight.z;

	BoundingBox initialViewSpaceBB = BoundingBox::NegativeInfinity();
	initialViewSpaceBB.Grow((camera.NearPlane * viewBottomLeft).xyz);
	initialViewSpaceBB.Grow((camera.NearPlane * viewBottomRight).xyz);
	initialViewSpaceBB.Grow((camera.NearPlane * viewTopLeft).xyz);
	initialViewSpaceBB.Grow((camera.NearPlane * viewTopRight).xyz);


	// World space.
	vec4 worldBottomLeft = vec4(camera.Rotation * viewBottomLeft.xyz, 0.f);
	vec4 worldBottomRight = vec4(camera.Rotation * viewBottomRight.xyz, 0.f);
	vec4 worldTopLeft = vec4(camera.Rotation * viewTopLeft.xyz, 0.f);
	vec4 worldTopRight = vec4(camera.Rotation * viewTopRight.xyz, 0.f);

	vec4 worldEye = vec4(camera.Position, 1.f);

	BoundingBox initialWorldSpaceBB = BoundingBox::NegativeInfinity();
	initialWorldSpaceBB.Grow((viewMatrix * (worldEye + camera.NearPlane * worldBottomLeft)).xyz);
	initialWorldSpaceBB.Grow((viewMatrix * (worldEye + camera.NearPlane * worldBottomRight)).xyz);
	initialWorldSpaceBB.Grow((viewMatrix * (worldEye + camera.NearPlane * worldTopLeft)).xyz);
	initialWorldSpaceBB.Grow((viewMatrix * (worldEye + camera.NearPlane * worldTopRight)).xyz);


	for (uint32 i = 0; i < NumShadowCascades; ++i)
	{
		float distance = CascadeDistances.data[i];

		BoundingBox worldBB = initialWorldSpaceBB;
		worldBB.Grow((viewMatrix * (worldEye + distance * worldBottomLeft)).xyz);
		worldBB.Grow((viewMatrix * (worldEye + distance * worldBottomRight)).xyz);
		worldBB.Grow((viewMatrix * (worldEye + distance * worldTopLeft)).xyz);
		worldBB.Grow((viewMatrix * (worldEye + distance * worldTopRight)).xyz);
		worldBB.Pad(0.1f);

		mat4 projMatrix;

		if (!preventRotationalShimmering)
		{
			projMatrix = CreateOrthographicProjectionMatrix(worldBB.MinCorner.x, worldBB.MaxCorner.x, worldBB.MaxCorner.y, worldBB.MinCorner.y,
				-worldBB.MaxCorner.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -worldBB.MinCorner.z);
		}
		else
		{
			BoundingBox viewBB = initialViewSpaceBB;
			viewBB.Grow((distance * viewBottomLeft).xyz);
			viewBB.Grow((distance * viewBottomRight).xyz);
			viewBB.Grow((distance * viewTopLeft).xyz);
			viewBB.Grow((distance * viewTopRight).xyz);
			viewBB.Pad(0.1f);

			vec3 viewSpaceCenter = viewBB.GetCenter();
			float radius = length(viewBB.GetRadius());

			vec3 center = (viewMatrix * vec4(camera.Rotation * viewSpaceCenter + camera.Position, 1.f)).xyz;

			projMatrix = CreateOrthographicProjectionMatrix(center.x + radius, center.x - radius, center.y + radius, center.y - radius,
				-worldBB.MaxCorner.z - SHADOW_MAP_NEGATIVE_Z_OFFSET, -worldBB.MinCorner.z);
		}

		ViewProj[i] = projMatrix * viewMatrix;

		// Move in pixel increments.
		// https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering

		vec4 shadowOrigin = (ViewProj[i] * vec4(0.f, 0.f, 0.f, 1.f)) * (float)ShadowDimensions * 0.5f;
		vec4 roundedOrigin = round(shadowOrigin);
		vec4 roundOffset = roundedOrigin - shadowOrigin;
		roundOffset = roundOffset * 2.f / (float)ShadowDimensions;
		roundOffset.z = 0.f;
		roundOffset.w = 0.f;

		ViewProj[i].col3 += roundOffset;
	}
}

mat4 GetSpotlightViewProjMatrix(const SpotLightCb &sl) {
	mat4 viewMatrix = LookAt(sl.Position, sl.Position + sl.Direction, vec3(0.f, 1.f, 0.f));
	mat4 projMatrix = CreatePerspectiveProjectionMatrix(acos(sl.GetOuterCutoff()) * 2.f, 1.f, 0.01f, sl.MaxDistance);
	return projMatrix * viewMatrix;
}
