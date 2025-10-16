#pragma once

#include "../core/math.h"

struct IndexedTriangle16 {
	uint16 A, B, C;
};

struct IndexedTriangle32 {
	uint32 A, B, C;
};

struct IndexedLine16 {
	uint16 A, B;
};

struct IndexedLine32 {
	uint32 A, B, C;
};

struct LineSegment {
	vec3 A, B;

	vec3 ClosestPoint(const vec3& q);
};

struct CapsuleCollider {
	vec3 PositionA;
	vec3 PositionB;
	float Radius;
};

union AABBCorners {
	struct {
		vec3 i;
		vec3 x;
		vec3 y;
		vec3 xy;
		vec3 z;
		vec3 xz;
		vec3 yz;
		vec3 xyz;
	};
	vec3 Corners[8];

	AABBCorners() {}
};

struct AABBCollider {
	vec3 MinCorner;
	vec3 MaxCorner;

	void Grow(vec3 o);
	void Pad(vec3 p);
	vec3 GetCenter() const;
	vec3 GetRadius() const;

	AABBCollider Transform(quat rotation, vec3 translation) const;
	AABBCorners GetCorners() const;
	AABBCorners GetCorners(quat rotation, vec3 translation) const;

	static AABBCollider NegativeInfinity();
	static AABBCollider FromMinMax(vec3 minCorner, vec3 maxCorner);
	static AABBCollider FromCenterRadius(vec3 center, vec3 radius);

	bool Collide(const AABBCollider& other);
	vec3 ClosestPoint(const vec3& q);
private:
	bool Collide(vec3 point);
};

struct ConvexHullCollider {
	vec3* Vertices;
	IndexedLine16* Triangles;
	uint32 NumVertices;
	uint32 NumTriangles;
};

struct PlaneCollider {
	vec3 Normal;
	float d;

	PlaneCollider() {}

	PlaneCollider(const vec3& point, const vec3& normal);
	float SignedDistance(const vec3& p) const;
	bool IsFrontFacingTo(const vec3& dir) const;
	vec3 GetPointOnPlane() const;
};

struct SphereCollider {
	vec3 Center;
	float Radius;

	bool Collide(const SphereCollider& other);
	bool Collide(const PlaneCollider& other);
};

enum ColliderType : uint16 {
	EColliderTypeSphere,
	EColliderTypeCapsule,
	EColliderTypeAabb
};

struct ColliderBase {
	union {
		SphereCollider sphere;
		CapsuleCollider capsule;
		AABBCollider aabb;
	};

	ColliderType type;
	uint16 rigidBodyId;

	ColliderBase() {}
};

struct ColliderProperties {
	float Restitution;
	float Friction;
	float Density;
};

struct Ray {
	vec3 Origin;
	vec3 Direction;

	bool IntersectPlane(vec3 normal, float d, float& outT) const;
	bool IntersectPlane(vec3 normal, vec3 point, float& outT) const;
	bool IntersectAABB(const AABBCollider& a, float& outT) const;
	bool IntersectTriangle(vec3 a, vec3 b, vec3 c, float& outT, bool& outFrontFacing) const;
	bool IntersectSphere(vec3 center, float radius, float& outT) const;
	bool IntersectSphere(const SphereCollider& sphere, float& outT) const {
		return IntersectSphere(sphere.Center, sphere.Radius, outT);
	}
};

float ClosestPointSegmentSegment(const LineSegment& l1, const LineSegment& l2, vec3& c1, vec3& c2);