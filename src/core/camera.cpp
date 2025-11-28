#include "camera.h"

void RenderCamera::InitializeIngame(vec3 position, quat rotation, float verticalFOV, float nearPlane, float farPlane) {
    Type = ECameraTypeIngame;
    Position = position;
    Rotation = rotation;
    this->verticalFOV = verticalFOV;
    NearPlane = nearPlane;
    FarPlane = farPlane;
}

void RenderCamera::InitializeCalibrated(vec3 position, quat rotation, uint32 width, uint32 height, float fx, float fy, float cx, float cy, float nearPlane, float farPlane) {
    Type = ECameraTypeCalibrated;
    Position = position;
    Rotation = rotation;
    FX = fx;
    FY = fy;
    CX = cx;
    CY = cy;
    Width = width;
    Height = height;
    NearPlane = nearPlane;
    FarPlane = farPlane;
}

void RenderCamera::SetViewport(uint32 width, uint32 height) {
    Width = width;
    Height = height;
    Aspect = (float)Width / (float)Height;
}

void RenderCamera::UpdateMatrices() {
    if (Type == ECameraTypeIngame) {
        Proj = CreatePerspectiveProjectionMatrix(verticalFOV, Aspect, NearPlane, FarPlane);
    }
    else {
        assert(Type == ECameraTypeCalibrated);
        Proj = CreatePerspectiveProjectionMatrix((float)Width, (float)Height, FX, FY, CX, CY, NearPlane, FarPlane);
    }

    InvProj = InvertPerspectiveProjectionMatrix(Proj);
    View = CreateViewMatrix(Position, Rotation);
    InvView = InvertedAffine(View);
    ViewProj = Proj * View;
    InvViewProj = InvView * InvProj;
}

CameraProjectionExtents RenderCamera::GetProjectionExtents() const {
    if (Type == ECameraTypeIngame) {
        float extentY = tanf(0.5f * verticalFOV);
        float extentX = extentY * Aspect;

        return CameraProjectionExtents{extentX, extentX, extentY, extentY};
    }
    assert(Type == ECameraTypeCalibrated);

    vec3 topLeft = RestoreViewSpacePosition(vec2(0.f, 0.f), 0.f) / NearPlane;
    vec3 bottomRight = RestoreViewSpacePosition(vec2(1.f, 1.f), 0.f) / NearPlane;

    return CameraProjectionExtents(-topLeft.x, bottomRight.x, topLeft.y, -bottomRight.y);
}

float RenderCamera::GetMinProjectionExtent() const {
    CameraProjectionExtents extents = GetProjectionExtents();
    float minHorizontalExtent = Min(extents.Left, extents.Right);
    float minVerticalExtent = Min(extents.Top, extents.Bottom);
    float minExtent = Min(minHorizontalExtent, minVerticalExtent);
    return minExtent;
}

Ray RenderCamera::GenerateWorldSpaceRay(float relX, float relY) const {
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

Ray RenderCamera::GenerateViewSpaceRay(float relX, float relY) const {
    float ndcX = 2.f * relX - 1.f;
    float ndcY = -(2.f * relY - 1.f);
    vec4 clip(ndcX, ndcY, -1.f, 1.f);
    vec4 eye = InvProj * clip;
    eye.z = -1.f;

    Ray result;
    result.Origin = vec3(0.f, 0.f, 0.f);
    result.Direction = normalize(eye.xyz);
    return result;
}

vec3 RenderCamera::RestoreViewSpacePosition(vec2 uv, float depthBufferDepth) const {
    uv.y = 1.f - uv.y;
    vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
    vec4 homPosition = InvProj * vec4(ndc, 1.f);
    vec3 position = homPosition.xyz / homPosition.w;
    return position;
}

vec3 RenderCamera::RestoreWorldSpacePosition(vec2 uv, float depthBufferDepth) const {
    uv.y = 1.f - uv.y;
    vec3 ndc = vec3(uv * 2.f - vec2(1.f, 1.f), depthBufferDepth);
    vec4 homPosition = InvProj * vec4(ndc, 1.f);
    vec3 position = homPosition.xyz / homPosition.w;
    return position;
}

float RenderCamera::DepthBufferDepthToEyeDepth(float depthBufferDepth) const {
    if (FarPlane < 0.f) { // Infinite far plane.
        depthBufferDepth = clamp(depthBufferDepth, 0.f, 1.f - 1e-7f); // A depth of 1 is at infinity.
        return -NearPlane / (depthBufferDepth - 1.f);
    }
    const float c1 = FarPlane / NearPlane;
    const float c0 = 1.f - c1;
    return FarPlane / (c0 * depthBufferDepth + c1);
}

float RenderCamera::EyeDepthToDepthBufferDepth(float eyeDepth) const {
    return -Proj.m22 + Proj.m23 / eyeDepth;
}

float RenderCamera::LinearizeDepthBuffer(float depthBufferDepth) const {
    assert(FarPlane > 0.f); // This is not possible with an infinite far plane.

    float eyeDepth = DepthBufferDepthToEyeDepth(depthBufferDepth);
    return (eyeDepth - NearPlane) / FarPlane;
}

CameraFrustumCorners RenderCamera::GetWorldSpaceFrustumCorners(float alternativeFarPlane) const {
    if (alternativeFarPlane <= 0.f) {
        alternativeFarPlane = FarPlane;
    }

    float depthValue = EyeDepthToDepthBufferDepth(alternativeFarPlane);

    CameraFrustumCorners result;

    result.NearBottomLeft = RestoreWorldSpacePosition(vec2(0.f, 1.f), 0.f);
    result.NearBottomRight = RestoreWorldSpacePosition(vec2(1.f, 1.f), 0.f);
    result.NearTopLeft = RestoreWorldSpacePosition(vec2(0.f, 0.f), 0.f);
    result.NearTopRight = RestoreWorldSpacePosition(vec2(1.f, 0.f), 0.f);
    result.FarBottomLeft = RestoreWorldSpacePosition(vec2(0.f, 1.f), depthValue);
    result.FarBottomRight = RestoreWorldSpacePosition(vec2(1.f, 1.f), depthValue);
    result.FarTopLeft = RestoreWorldSpacePosition(vec2(0.f, 0.f), depthValue);
    result.FarTopRight = RestoreWorldSpacePosition(vec2(1.f, 0.f), depthValue);

    return result;
}

CameraFrustumPlanes GetWorldSpaceFrustumPlanes(const mat4 &viewProj) {
    CameraFrustumPlanes result;

    vec4 c0(viewProj.m00, viewProj.m01, viewProj.m02, viewProj.m03);
    vec4 c1(viewProj.m10, viewProj.m11, viewProj.m12, viewProj.m13);
    vec4 c2(viewProj.m20, viewProj.m21, viewProj.m22, viewProj.m23);
    vec4 c3(viewProj.m30, viewProj.m31, viewProj.m32, viewProj.m33);

    result.LeftPlane = c3 + c0;
    result.RightPlane = c3 - c0;
    result.TopPlane = c3 - c1;
    result.BottomPlane = c3 + c1;
    result.NearPlane = c2;
    result.FarPlane = c3 - c2;

    return result;
}

CameraFrustumPlanes RenderCamera::GetWorldSpaceFrustumPlanes() const {
    return ::GetWorldSpaceFrustumPlanes(ViewProj);
}

CameraFrustumCorners RenderCamera::GetViewSpaceFrustumCorners(float alternativeFarPlane) const {
    if (alternativeFarPlane <= 0.f) {
        alternativeFarPlane = FarPlane;
    }

    float depthValue = EyeDepthToDepthBufferDepth(alternativeFarPlane);

    CameraFrustumCorners result;

    result.eye = Position;

    result.NearBottomLeft = RestoreViewSpacePosition(vec2(0.f, 1.f), 0.f);
    result.NearBottomRight = RestoreViewSpacePosition(vec2(1.f, 1.f), 0.f);
    result.NearTopLeft = RestoreViewSpacePosition(vec2(0.f, 0.f), 0.f);
    result.NearTopRight = RestoreViewSpacePosition(vec2(1.f, 0.f), 0.f);
    result.FarBottomLeft = RestoreViewSpacePosition(vec2(0.f, 1.f), depthValue);
    result.FarBottomRight = RestoreViewSpacePosition(vec2(1.f, 1.f), depthValue);
    result.FarTopLeft = RestoreViewSpacePosition(vec2(0.f, 0.f), depthValue);
    result.FarTopRight = RestoreViewSpacePosition(vec2(1.f, 0.f), depthValue);

    return result;
}

RenderCamera RenderCamera::GetJitteredVersion(vec2 offset) const {
    CameraProjectionExtents extents = GetProjectionExtents();
    float texelSizeX = (extents.Left + extents.Right) / Width;
    float texelSizeY = (extents.Top + extents.Bottom) / Height;

    float jitterX = texelSizeX * offset.x;
    float jitterY = texelSizeY * offset.y;

    float left = jitterX - extents.Left;
    float right = jitterX + extents.Right;
    float bottom = jitterY - extents.Bottom;
    float top = jitterY + extents.Top;

    mat4 jitteredProj = CreatePerspectiveProjectionMatrix(right * NearPlane, left * NearPlane, top * NearPlane, bottom * NearPlane, NearPlane, FarPlane);

    RenderCamera result = *this;

    result.Proj = jitteredProj;
    result.InvProj = invert(jitteredProj);
    result.ViewProj = jitteredProj * View;
    result.InvViewProj = InvView * result.InvProj;

    return result;
}

bool CameraFrustumPlanes::CullWorldSpaceAABB(const BoundingBox &aabb) const {
    for (uint32 i = 0; i < 6; ++i) {
        vec4 plane = planes[i];
        vec4 vertex(
            (plane.x < 0.f) ? aabb.MinCorner.x : aabb.MaxCorner.x,
            (plane.y < 0.f) ? aabb.MinCorner.y : aabb.MaxCorner.y,
            (plane.z < 0.f) ? aabb.MinCorner.z : aabb.MaxCorner.z,
            1.f
        );
        if (dot(plane, vertex) < 0.f) {
            return true;
        }
    }
    return false;
}

bool CameraFrustumPlanes::CullModelSpaceAABB(const BoundingBox &aabb, const trs &transform) const {
    return CullModelSpaceAABB(aabb, trsToMat4(transform));
}

bool CameraFrustumPlanes::CullModelSpaceAABB(const BoundingBox &aabb, const mat4 &transform) const {
    // TODO: Transform planes instead of AABB?

    vec4 worldSpaceCorners[] = {transform * vec4(aabb.MinCorner.x, aabb.MinCorner.y, aabb.MinCorner.z, 1.f),
        transform * vec4(aabb.MaxCorner.x, aabb.MinCorner.y, aabb.MinCorner.z, 1.f),
        transform * vec4(aabb.MinCorner.x, aabb.MaxCorner.y, aabb.MinCorner.z, 1.f),
        transform * vec4(aabb.MaxCorner.x, aabb.MaxCorner.y, aabb.MinCorner.z, 1.f),
        transform * vec4(aabb.MinCorner.x, aabb.MinCorner.y, aabb.MaxCorner.z, 1.f),
        transform * vec4(aabb.MaxCorner.x, aabb.MinCorner.y, aabb.MaxCorner.z, 1.f),
        transform * vec4(aabb.MinCorner.x, aabb.MaxCorner.y, aabb.MaxCorner.z, 1.f),
        transform * vec4(aabb.MaxCorner.x, aabb.MaxCorner.y, aabb.MaxCorner.z, 1.f),
    };

    for (uint32 i = 0; i < 6; ++i) {
        vec4 plane = planes[i];

        bool inside = false;

        for (uint32 j = 0; j < 8; ++j) {
            if (dot(plane, worldSpaceCorners[j]) > 0.f) {
                inside = true;
                break;
            }
        }

        if (!inside) {
            return true;
        }
    }

    return false;
}


