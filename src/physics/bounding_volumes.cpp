#include "bounding_volumes.h"

namespace {
    bool IsZero(float x) {
        return abs(x) < 1e-6f;
    }
}

bool BoundingBox::Collide(vec3 point) {
    return  point.x >= MinCorner.x
        and point.y >= MinCorner.y
        and point.z >= MinCorner.z
        and point.x <= MaxCorner.x
        and point.y <= MaxCorner.y
        and point.z <= MaxCorner.z;
}

void BoundingBox::Grow(vec3 o) {
    MinCorner.x = Min(MinCorner.x, o.x);
    MinCorner.y = Min(MinCorner.y, o.y);
    MinCorner.z = Min(MinCorner.z, o.z);
    MaxCorner.x = Max(MaxCorner.x, o.x);
    MaxCorner.y = Max(MaxCorner.y, o.y);
    MaxCorner.z = Max(MaxCorner.z, o.z);
}

void BoundingBox::Pad(vec3 p) {
    MinCorner -= p;
    MaxCorner += p;
}

vec3 BoundingBox::GetCenter() const {
    return (MinCorner + MaxCorner) * 0.5f;
}

vec3 BoundingBox::GetRadius() const {
    return (MaxCorner - MinCorner) * 0.5f;
}

BoundingBox BoundingBox::Transform(quat rotation, vec3 translation) const {
    BoundingBox result = BoundingBox::NegativeInfinity();
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

BoundingBoxCorners BoundingBox::GetCorners() const {
    BoundingBoxCorners result;
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

BoundingBoxCorners BoundingBox::GetCorners(quat rotation, vec3 translation) const {
    BoundingBoxCorners result;
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

BoundingBox BoundingBox::NegativeInfinity() {
    return BoundingBox{vec3(FLT_MAX, FLT_MAX, FLT_MAX), vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX)};
}

BoundingBox BoundingBox::FromMinMax(vec3 minCorner, vec3 maxCorner) {
    return BoundingBox{minCorner, maxCorner};
}

BoundingBox BoundingBox::FromCenterRadius(vec3 center, vec3 radius) {
    return BoundingBox{center - radius, center + radius};
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

bool Ray::IntersectAABB(const BoundingBox &a, float &outT) const {
    vec3 invDir = vec3(1.f / Direction.x, 1.f / Direction.y, 1.f / Direction.z); // This can be Inf (when one direction component is 0) but still works.

    float tx1 = (a.MinCorner.x - Origin.x) * invDir.x;
    float tx2 = (a.MaxCorner.x - Origin.x) * invDir.x;

    outT = Min(tx1, tx2);
    float tmax = Max(tx1, tx2);

    float ty1 = (a.MinCorner.y - Origin.y) * invDir.y;
    float ty2 = (a.MaxCorner.y - Origin.y) * invDir.y;

    outT = Max(outT, Min(ty1, ty2));
    tmax = Min(tmax, Max(ty1, ty2));

    float tz1 = (a.MaxCorner.z - Origin.z) * invDir.z;
    float tz2 = (a.MaxCorner.z - Origin.z) * invDir.z;

    outT = Max(outT, Min(tz1, tz2));
    tmax = Min(tmax, Max(tz1, tz2));

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

bool Ray::IntersectCylinder(const BoundingCylinder &cylinder, float &outT) const {
    vec3 d = Direction;
    vec3 o = Origin;

    vec3 axis = cylinder.PositionB - cylinder.PositionA;
    float height = length(axis);

    quat q = rotateFromTo(axis, vec3(0.f, 1.f, 0.f));

    vec3 posA = cylinder.PositionA;
    vec3 posB = posA + vec3(0.f, height, 0.f);

    o = q * (o - posA);
    d = q * d;

    float a = d.x * d.x + d.z * d.z;
    float b = d.x * o.x + d.z * o.z;
    float c = o.x * o.x + o.z * o.z - cylinder.Radius * cylinder.Radius;

    float delta = b * b - a * c;

    float epsilon = 1e-6f;

    if (delta < epsilon) {
        return false;
    }

    outT = (-b - sqrt(delta)) / a;
    if (outT <= epsilon) {
        return false; // Behind ray.
    }


    float y = o.y + outT * d.y;

    // Check bases.
    if (y > height + epsilon || y < -epsilon) {
        Ray localRay = { o, d };

        float dist;
        bool b1 = localRay.IntersectDisk(posB, vec3(0.f, 1.f, 0.f), cylinder.Radius, dist);
        if (b1) {
            outT = dist;
        }
        bool b2 = localRay.IntersectDisk(posA, vec3(0.f, -1.f, 0.f), cylinder.Radius, dist);
        if (b2 && dist > epsilon && outT >= dist) {
            outT = dist;
        }
    }

    y = o.y + outT * d.y;

    return y > -epsilon && y < height + epsilon;
}

bool Ray::IntersectDisk(vec3 pos, vec3 normal, float radius, float &outT) const {
    bool intersectsPlane = IntersectPlane(normal, pos, outT);
    if (intersectsPlane) {
        return length(Origin + outT * Direction - pos) <= radius;
    }
    return false;
}

bool Ray::IntersectRectangle(vec3 pos, vec3 tangent, vec3 bitangent, vec2 radius, float &outT) const {
    vec3 normal = cross(tangent, bitangent);
    bool intersectsPlane = IntersectPlane(normal, pos, outT);
    if (intersectsPlane) {
        vec3 offset = Origin + outT * Direction - pos;
        vec2 proj(dot(offset, tangent), dot(offset, bitangent));
        proj = abs(proj);
        if (proj.x <= radius.x and proj.y <= radius.y) {
            return true;
        }
    }
    return false;
}

struct Solve2Result {
	uint32 NumResults;
	float Results[2];
};

struct Solve3Result {
	uint32 NumResults;
	float Results[3];
};

struct Solve4Result {
	uint32 NumResults;
	float Results[4];
};

static Solve2Result Solve2(float c0, float c1, float c2) {
	float p = c1 / (2 * c2);
	float q = c0 / c2;

	float D = p * p - q;

	if (IsZero(D)) {
		return { 1, -p };
	}
	if (D < 0) {
		return { 0 };
	}
	float sqrt_D = sqrt(D);

	return { 2, sqrt_D - p, -sqrt_D - p };
}

static Solve3Result Solve3(float c0, float c1, float c2, float c3) {
	float A = c2 / c3;
	float B = c1 / c3;
	float C = c0 / c3;

	float sq_A = A * A;
	float p = 1.f / 3 * (-1.f / 3 * sq_A + B);
	float q = 1.f / 2 * (2.f / 27 * A * sq_A - 1.f / 3 * A * B + C);

	/* use Cardano's formula */

	float cb_p = p * p * p;
	float D = q * q + cb_p;

	Solve3Result s = {};

	if (IsZero(D)) {
		if (IsZero(q)) {
			s = { 1, 0.f };
		}
		else {
			float u = cbrt(-q);
			s = { 2, 2.f * u, -u };
		}
	}
	else if (D < 0) { /* Casus irreducibilis: three real solutions */
		float phi = 1.f / 3.f * acos(-q / sqrt(-cb_p));
		float t = 2.f * sqrt(-p);

		s = { 3,
			t * cos(phi),
			-t * cos(phi + PI / 3),
			-t * cos(phi - PI / 3) };
	}
	else { /* one real solution */
		float sqrt_D = sqrt(D);
		float u = cbrt(sqrt_D - q);
		float v = -cbrt(sqrt_D + q);

		s = { 1, u + v };
	}

	/* resubstitute */

	float sub = 1.f / 3.f * A;

	for (uint32 i = 0; i < s.NumResults; ++i) {
		s.Results[i] -= sub;
	}

	return s;
}

/**
 *  Solves equation:
 *
 *      c[0] + c[1]*x + c[2]*x^2 + c[3]*x^3 + c[4]*x^4 = 0
 *
 */
static Solve4Result solve4(float c0, float c1, float c2, float c3, float c4) {
	/* normal form: x^4 + Ax^3 + Bx^2 + Cx + D = 0 */

	float A = c3 / c4;
	float B = c2 / c4;
	float C = c1 / c4;
	float D = c0 / c4;

	/*  substitute x = y - A/4 to eliminate cubic term:
	x^4 + px^2 + qx + r = 0 */

	float sq_A = A * A;
	float p = -3.f / 8 * sq_A + B;
	float q = 1.f / 8 * sq_A * A - 1.f / 2 * A * B + C;
	float r = -3.f / 256 * sq_A * sq_A + 1.f / 16 * sq_A * B - 1.f / 4 * A * C + D;
	Solve4Result s = {};

	if (IsZero(r)) {
		/* no absolute term: y(y^3 + py + q) = 0 */

		auto s3 = Solve3(q, p, 0, 1);
		for (uint32 i = 0; i < s3.NumResults; ++i) {
			s.Results[s.NumResults++] = s3.Results[i];
		}

		s.Results[s.NumResults++] = 0.f;
	}
	else {
		/* solve the resolvent cubic ... */

		auto s3 = Solve3(1.f / 2 * r * p - 1.f / 8 * q * q, -r, -0.5f * p, 1.f);
		for (uint32 i = 0; i < s3.NumResults; ++i) {
			s.Results[s.NumResults++] = s3.Results[i];
		}

		/* ... and take the one real solution ... */

		float z = s.Results[0];

		/* ... to build two quadric equations */

		float u = z * z - r;
		float v = 2.f * z - p;

		if (IsZero(u)) {
			u = 0;
		}
		else if (u > 0)	{
			u = sqrt(u);
		}
		else {
			return {};
		}

		if (IsZero(v)) {
			v = 0;
		}
		else if (v > 0)	{
			v = sqrt(v);
		}
		else {
			return {};
		}

		auto s2 = Solve2(z - u, q < 0 ? -v : v, 1);
		s = {};
		for (uint32 i = 0; i < s2.NumResults; ++i) {
			s.Results[s.NumResults++] = s2.Results[i];
		}

		s2 = Solve2(z + u, q < 0 ? v : -v, 1);
		for (uint32 i = 0; i < s2.NumResults; ++i) {
			s.Results[s.NumResults++] = s2.Results[i];
		}
	}

	/* resubstitute */

	float sub = 1.f / 4 * A;

	for (uint32 i = 0; i < s.NumResults; ++i) {
		s.Results[i] -= sub;
	}

	return s;
}

bool Ray::IntersectTorus(const BoundingTorus &torus, float &outT) const {
	vec3 d = Direction;
	vec3 o = Origin;

	vec3 axis = torus.UpAxis;

	quat q = rotateFromTo(axis, vec3(0.f, 1.f, 0.f));

	o = q * (o - torus.Position);
	d = q * d;



	// define the coefficients of the quartic equation
	float sum_d_sqrd = dot(d, d);

	float e = dot(o, o) - torus.MajorRadius * torus.MajorRadius - torus.TubeRadius * torus.TubeRadius;
	float f = dot(o, d);
	float four_a_sqrd = 4.f * torus.MajorRadius * torus.MajorRadius;

	auto solution = solve4(
		e * e - four_a_sqrd * (torus.TubeRadius * torus.TubeRadius - o.y * o.y),
		4.f * f * e + 2.f * four_a_sqrd * o.y * d.y,
		2.f * sum_d_sqrd * e + 4.f * f * f + four_a_sqrd * d.y * d.y,
		4.f * sum_d_sqrd * f,
		sum_d_sqrd * sum_d_sqrd
	);

	// ray misses the torus
	if (solution.NumResults == 0) {
		return false;
	}

	// find the smallest root greater than kEpsilon, if any
	// the roots array is not sorted
	float minT = FLT_MAX;
	for (uint32 i = 0; i < solution.NumResults; ++i) {
		float t = solution.Results[i];
		if ((t > 1e-6f) && (t < minT)) {
			minT = t;
		}
	}
	outT = minT;
	return true;
}

float SignedDistanceToPlane(const vec3& p, const vec4& plane) {
	return dot(vec4(p, 1.f), plane);
}

bool BoundingBox::Collide(const BoundingBox &other) {
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

bool BoundingSphere::Collide(const BoundingSphere &sphere) {
    vec3 d = Center - sphere.Center;
    float dist2 = dot(d, d);
    float radiusSum = Radius + sphere.Radius;
    return dist2 <= radiusSum * radiusSum;
}

bool BoundingSphere::Collide(const vec4& plane) {
    return fabs(SignedDistanceToPlane(Center, plane)) <= Radius;
}

vec3 LineSegment::ClosestPoint(const vec3 &q) {
    vec3 ab = B - A;
    float t = dot(q - A, ab) / squaredLength(ab);
    t = clamp(t, 0.f, 1.f);
    return A + t * ab;
}

vec3 BoundingBox::ClosestPoint(const vec3 &q) {
    vec3 result;
    for (int i = 0; i < 3; i++) {
        float v = q.data[i];
        if (v < MinCorner.data[i]) {
            v = MinCorner.data[i];
        }
        if (v > MaxCorner.data[i]) {
            v = MaxCorner.data[i];
        }
        result.data[i] = v;
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

