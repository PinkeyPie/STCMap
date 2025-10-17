#include "camera.h"

void RenderCamera::RecalculateMatrices(float renderWidth, float renderHeight) {
	const float aspect = renderWidth / renderHeight;
	Proj = CreatePerspectiveProjectionMatrix(VerticalFOV, aspect, NearPlane, FarPlane);
	InvProj = InvertPerspectiveProjectionMatrix(Proj);
	View = CreateViewMatrix(Position, Rotation);
	InvView = InvertedAffine(View);
	ViewProj = Proj * View;
	InvViewProj = InvView * InvProj;
}

void RenderCamera::RecalculateMatrices(uint32 renderWidth, uint32 renderHeight) {
	return RecalculateMatrices((float)renderWidth, (float)renderHeight);
}

Ray CameraBase::GenerateWorldSpaceRay(float relX, float relY) const {
	float ndcX = 2.f * relX - 1.f;
	float ndcY = -(2.f * relY - 1.f);
	vec4 clip(ndcX, ndcY, -1.f, 1.f);
	vec4 eye = InvProj * clip;
	eye.z = -1.f; eye.w = 0.f;

	Ray result;
	result.Origin = Position;
	result.Direction = normalize((InvView * eye).xyz);
	return result;
}

Ray CameraBase::GenerateViewSpaceRay(float relX, float relY) const {
	float ndcX = 2.f * relX - 1.f;
	float ndcY = -(2.f * relY - 1.f);
	vec4 clip(ndcX, ndcY, -1.f, 1.f);
	vec4 eye = InvProj * clip;
	eye.z = -1.f; eye.w = 0.f;

	Ray result;
	result.Origin = vec3(0.f, 0.f, 0.f);
	result.Direction = normalize(eye.xyz);
	return result;
}

void RealCamera::RecalculateMatrices() {
	float aspect = (float)Width / (float)Height;
	Proj = CreatePerspectiveProjectionMatrix((float)Width, (float)Height, Intr.Fx, Intr.Fy, Intr.Cx, Intr.Cy, NearPlane, FarPlane);
	InvProj = InvertPerspectiveProjectionMatrix(Proj);
	View = CreateViewMatrix(Position, Rotation);
	InvView = InvertedAffine(View);
	ViewProj = Proj * View;
	InvViewProj = InvView * InvProj;
}

