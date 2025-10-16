#include "colliders.h"

namespace {
    float ClosestPointOnLineSegment(vec3 point, vec3 lineA, vec3 lineB) {
        vec3 ab = lineB - lineA;
        float t = dot(point - lineA, ab) / squaredLength(ab);
        t = clamp(t, 0.f, 1.f);
        return t;
    }
}

bool AABBCollider::Collide(vec3 point) {
    return  point.x >= MinCorner.x
        and point.y >= MinCorner.y
        and point.z >= MinCorner.z
        and point.x <= MaxCorner.x
        and point.y <= MaxCorner.y
        and point.z <= MaxCorner.z;
}

void AABBCollider::Grow(vec3 o) {
    MinCorner.x = min(MinCorner.x, o.x);
    MinCorner.y = min(MinCorner.y, o.y);
    MinCorner.z = min(MinCorner.z, o.z);
    MaxCorner.x = max(MaxCorner.x, o.x);
    MaxCorner.y = max(MaxCorner.y, o.y);
    MaxCorner.z = max(MaxCorner.z, o.z);
}

void AABBCollider::Pad(vec3 p) {
    MinCorner -= p;
    MaxCorner += p;
}

vec3 AABBCollider::GetCenter() const {
    return (MinCorner + MaxCorner) * 0.5f;
}

vec3 AABBCollider::GetRadius() const {
    return (MaxCorner - MinCorner) * 0.5f;
}

AABBCollider AABBCollider::Transform(quat rotation, vec3 translation) const {
    AABBCollider result = AABBCollider::NegativeInfinity();
    result.Grow(rotation * MinCorner + translation);
    result.Grow(rotation * vec3(MaxCorner.x, MinCorner.y, MinCorner.z) + translation);
    result.Grow(rotation * vec3(MinCorner.x, MaxCorner.y, MinCorner.z) + translation);
    result.Grow(rotation * vec3(MaxCorner.x, MaxCorner.y, MinCorner.z) + translation);
    result.Grow(rotation * vec3(MinCorner.x, MinCorner.y, MaxCorner.z) + translation);
    result.Grow(rotation * vec3(MaxCorner.x, MinCorner.y, MaxCorner.z) + translation);
    result.Grow(rotation * vec3(MinCorner.x, MaxCorner.y, MaxCorner.z) + translation);
    result.Grow(rotation * MaxCorner + translation);
    return result;
}

AABBCorners AABBCollider::GetCorners() const {
    AABBCorners result;
    result.i = MinCorner;
    result.x = vec3(MaxCorner.x, MinCorner.y, MinCorner.z);
    result.y = vec3(MaxCorner.x, MaxCorner.y, MinCorner.z);
    result.xy = vec3(MaxCorner.x, MaxCorner.y, MinCorner.z);
    result.z = vec3(MinCorner.x, MinCorner.y, MaxCorner.z);
    result.xz = vec3(MaxCorner.x, MinCorner.y, MaxCorner.z);
    result.yz = vec3(MinCorner.x, MaxCorner.y, MaxCorner.z);
    result.xyz = MaxCorner;
    return result;
}

AABBCorners AABBCollider::GetCorners(quat rotation, vec3 translation) const {
    AABBCorners result;
    result.i = rotation * MinCorner + translation;
    result.x = rotation * vec3(MaxCorner.x, MinCorner.y, MinCorner.z) + translation;
    result.y = rotation * vec3(MinCorner.x, MaxCorner.y, MinCorner.z) + translation;
    result.xy = rotation * vec3(MaxCorner.x, MaxCorner.y, MinCorner.z) + translation;
    result.z = rotation * vec3(MinCorner.x, MinCorner.y, MaxCorner.z) + translation;
    result.xz = rotation * vec3(MaxCorner.x, MinCorner.y, MaxCorner.z) + translation;
    result.yz = rotation * vec3(MinCorner.x, MaxCorner.y, MaxCorner.z) + translation;
    result.xyz = rotation * MaxCorner + translation;
    return result;
}

AABBCollider AABBCollider::NegativeInfinity() {
    return AABBCollider{vec3(FLT_MAX, FLT_MAX, FLT_MAX), vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX)};
}

AABBCollider AABBCollider::FromMinMax(vec3 minCorner, vec3 maxCorner) {
    return AABBCollider{minCorner, maxCorner};
}

AABBCollider AABBCollider::FromCenterRadius(vec3 center, vec3 radius) {
    return AABBCollider{center - radius, center + radius};
}

PlaneCollider::PlaneCollider(const vec3 &point, const vec3 &normal) : Normal(normal) {
    d = -dot(normal, point);
}

float PlaneCollider::SignedDistance(const vec3 &p) const {
    return dot(Normal, p) + d;
}

bool PlaneCollider::IsFrontFacingTo(const vec3 &dir) const {
    return dot(Normal, dir) <= 0.f;
}

vec3 PlaneCollider::GetPointOnPlane() const {
    return -d * Normal;
}

bool Ray::IntersectPlane(vec3 normal, float d, float &outT) const {
    float ndotd = dot(Direction, normal);
    if (abs(ndotd) < 1e-6f) {
        return false;
    }

    outT = -(dot(Origin, normal) + d) / dot(Direction, normal);
    return true;
}

bool Ray::IntersectPlane(vec3 normal, vec3 point, float &outT) const {
    float d = -dot(normal, point);
    return IntersectPlane(normal, d, outT);
}

bool Ray::IntersectAABB(const AABBCollider &a, float &outT) const {
    vec3 invDir = vec3(1.f / Direction.x, 1.f / Direction.y, 1.f / Direction.z); // This can be Inf (when one direction component is 0) but still works.

    float tx1 = (a.MinCorner.x - Origin.x) * invDir.x;
    float tx2 = (a.MaxCorner.x - Origin.x) * invDir.x;

    outT = min(tx1, tx2);
    float tmax = max(tx1, tx2);

    float ty1 = (a.MinCorner.y - Origin.y) * invDir.y;
    float ty2 = (a.MaxCorner.y - Origin.y) * invDir.y;

    outT = max(outT, min(ty1, ty2));
    tmax = min(tmax, max(ty1, ty2));

    float tz1 = (a.MaxCorner.z - Origin.z) * invDir.z;
    float tz2 = (a.MaxCorner.z - Origin.z) * invDir.z;

    outT = max(outT, min(tz1, tz2));
    tmax = min(tmax, max(tz1, tz2));

    bool result = tmax >= outT and outT > 0.f;

    return result;
}

bool Ray::IntersectTriangle(vec3 a, vec3 b, vec3 c, float &outT, bool &outFrontFacing) const {
    vec3 normal = noz(cross(b - a, c - a));
    float d = -dot(normal, a);

    float nDotR = dot(Direction, normal);
    if (fabsf(nDotR) <= 1e-6f) {
        return false;
    }

    outT = -(dot(Origin, normal) + d) / nDotR;

    vec3 q = Origin + outT * Direction;
    outFrontFacing = nDotR < 0.f;
    return outT >= 0.f and PointInTriangle(q, a, b, c);
}

bool Ray::IntersectSphere(vec3 center, float radius, float &outT) const {
    vec3 m = Origin - center;
    float b = dot(m, Direction);
    float c = dot(m, m) - radius * radius;

    if (c > 0.f and b > 0.f) {
        return false;
    }

    float discr = b * b - c;

    if (discr < 0.f) {
        return false;
    }

    outT = -b - sqrt(discr);

    if (outT < 0.f) {
        outT = 0.f;
    }

    return true;
}

bool AABBCollider::Collide(const AABBCollider &other) {
    if (MaxCorner.x < other.MinCorner.x or MinCorner.x > other.MaxCorner.x) {
        return false;
    }
    if (MaxCorner.z < other.MinCorner.z or MinCorner.z > other.MaxCorner.z) {
        return false;
    }
    if (MaxCorner.y < other.MinCorner.y or MinCorner.y > other.MaxCorner.y) {
        return false;
    }
    return true;
}

bool SphereCollider::Collide(const SphereCollider &other) {
    vec3 d = Center - other.Center;
    float dist2 = dot(d, d);
    float radiusSum = Radius + other.Radius;
    return dist2 <= radiusSum * radiusSum;
}

bool SphereCollider::Collide(const PlaneCollider &other) {
    return fabs(other.SignedDistance(Center)) <= Radius;
}

vec3 LineSegment::ClosestPoint(const vec3 &q) {
    vec3 ab = B - A;
    float t = dot(q - A, ab) / squaredLength(ab);
    t = clamp(t, 0.f, 1.f);
    return A + t * ab;
}

vec3 AABBCollider::ClosestPoint(const vec3 &q) {
    vec3 result;
    for (int i = 0; i < 3; i++) {
        float v = q.e[i];
        if (v < MinCorner.e[i]) {
            v = MinCorner.e[i];
        }
        if (v > MaxCorner.e[i]) {
            v = MaxCorner.e[i];
        }
        result.e[i] = v;
    }
    return result;
}

float ClosestPointSegmentSegment(const LineSegment &l1, const LineSegment &l2, vec3 &c1, vec3 &c2) {
    float s, t;
    vec3 d1 = l1.A - l1.B; // Direction vector of segment S1
    vec3 d2 = l2.B - l2.A; // Direction vector of segment S2
    vec3 r = l1.A - l2.A;
    float a = dot(d1, d1); // Squared length of segment S1, always nonnegative
    float e = dot(d2, d2); // Squared length of segment S2, always nonnegative
    float f = dot(d2, r);
    // Check if either or both segments degenerate into points
    if (a <= EPSILON and e <= EPSILON) {
        // Both segments degenerate into points
        s = t = 0.f;
        c1 = l1.A;
        c2 = l2.A;
        return dot(c1 - c2, c1 - c2);
    }
    if (a <= EPSILON) {
        // First segment degenerates into a point
        s = 0.f;
        t = f / e; // s = 0 => t = (b * s + f) / e = f / e
        t = clamp(t, 0.f, 1.f);
    }
    else {
        float c = dot(d1, r);
        if (e <= EPSILON) {
            // Second segment degenerates into a point
            t = 0.f;
            s = clamp(-c / a, 0.f, 1.f); // t = 0 => s = (b * t - c) / a = -c / a
        }
        else {
            // The general nondegenerate case starts here
            float b = dot(d1, d2);
            float denominator = a * e - b * b; // Always nonnegative
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary s (here 0)
            if (denominator != 0.f) {
                s = clamp((b * f - c * e) / denominator, 0.f, 1.f);
            }
            else {
                s = 0.f;
            }
            // Compute point on L2 closest to S1(s) using
            t = (b * s + f) / e;
            // If t in [0, 1] done, Else clamp t, recompute s for the new value
            // of t using s = dot((l2.A + D2 * t) - l1.A, D1) / dot(D1, D1) = (t * b - c) / a
            // and clamp s to [0, 1]
            if (t < 0.f) {
                t = 0.f;
                s = clamp(-c / a, 0.f, 1.f);
            }
            else if (t > 1.f) {
                t = 1.f;
                s = clamp((b - c) / a, 0.f, 1.f);
            }
        }
    }
    c1 = l1.A + d1 * s;
    c2 = l2.A + d2 * t;
    return squaredLength(c1 - c2);
}

