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

struct BoundingSphere {
	vec3 Center;
	float Radius;

	bool Collide(const BoundingSphere& other);
	bool Collide(const vec4& plane);
};

struct BoundingCapsule {
	vec3 PositionA;
	vec3 PositionB;
	float Radius;
};

struct BoundingCylinder {
	vec3 PositionA;
	vec3 PositionB;
	float Radius;
};

struct BoundingTorus {
	vec3 Position;
	vec3 UpAxis;
	float MajorRadius;
	float TubeRadius;
};

union BoundingBoxCorners {
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

	BoundingBoxCorners() {}
};

struct BoundingBox {
	vec3 MinCorner;
	vec3 MaxCorner;

	void Grow(vec3 o);
	void Pad(vec3 p);
	vec3 GetCenter() const;
	vec3 GetRadius() const;

	BoundingBox Transform(quat rotation, vec3 translation) const;
	BoundingBoxCorners GetCorners() const;
	BoundingBoxCorners GetCorners(quat rotation, vec3 translation) const;

	static BoundingBox NegativeInfinity();
	static BoundingBox FromMinMax(vec3 minCorner, vec3 maxCorner);
	static BoundingBox FromCenterRadius(vec3 center, vec3 radius);

	bool Collide(const BoundingBox& other);
	vec3 ClosestPoint(const vec3& q);
private:
	bool Collide(vec3 point);
};

struct BoundingHull {
	vec3* Vertices;
	IndexedLine16* Triangles;
	uint32 NumVertices;
	uint32 NumTriangles;
};

struct Ray {
	vec3 Origin;
	vec3 Direction;

	bool IntersectPlane(vec3 normal, float d, float& outT) const;
	bool IntersectPlane(vec3 normal, vec3 point, float& outT) const;
	bool IntersectAABB(const BoundingBox& a, float& outT) const;
	bool IntersectTriangle(vec3 a, vec3 b, vec3 c, float& outT, bool& outFrontFacing) const;
	bool IntersectSphere(vec3 center, float radius, float& outT) const;
	bool IntersectSphere(const BoundingSphere& sphere, float& outT) const { return IntersectSphere(sphere.Center, sphere.Radius, outT); }
	bool IntersectCylinder(const BoundingCylinder& cylinder, float& outT) const;
	bool IntersectDisk(vec3 pos, vec3 normal, float radius, float& outT) const;
	bool IntersectRectangle(vec3 pos, vec3 tangent, vec3 bitangent, vec2 radius, float& outT) const;
	bool IntersectTorus(const BoundingTorus& torus, float& outT) const;
};

inline vec4 CreatePlane(vec3 point, vec3 normal) {
	float d = -dot(normal, point);
	return vec4(normal, d);
}

float SignedDistanceToPlane(const vec3& p, const vec4& plane);

float ClosestPointSegmentSegment(const LineSegment& l1, const LineSegment& l2, vec3& c1, vec3& c2);
