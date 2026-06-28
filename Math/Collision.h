#pragma once
#include "MathTypes.h"
#include "VectorMath.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

struct Sphere {
	Vector3 center;
	float radius;
};

struct Line {
	Vector3 origin;
	Vector3 diff;
};

struct Ray {
	Vector3 origin;
	Vector3 diff;
};

struct Segment {
	Vector3 origin;
	Vector3 diff;
};

struct Plane {
	Vector3 normal;
	float distance;
};

struct CollisionTriangle {
	Vector3 vertices[3];
};

struct AABB {
	Vector3 min;
	Vector3 max;
};

struct OBB {
	Vector3 center;
	Vector3 Orientation[3];
	Vector3 Size;
};

class Collision {
public:
	static Vector3 Project(const Vector3& v1, const Vector3& v2);
	static Vector3 ClosestPoint(const Vector3& point, const Segment& segment);
	static OBB     AABBToOBB(const AABB& aabb);

	static bool SphereSphere(const Sphere& s1, const Sphere& s2);
	static bool SpherePlane(const Sphere& sphere, const Plane& plane);
	static bool LinePlane(const Line& line, const Plane& plane, float& out_t, Vector3& out_intersection);
	static bool SegmentPlane(const Segment& segment, const Plane& plane, float& out_t, Vector3& out_intersection);
	static bool RayPlane(const Ray& ray, const Plane& plane, float& out_t, Vector3& out_intersection);
	static bool TriangleSegment(const CollisionTriangle& triangle, const Segment& segment, float& out_t);
	static bool TriangleLine(const CollisionTriangle& triangle, const Line& line, float& out_t);
	static bool TriangleRay(const CollisionTriangle& triangle, const Ray& ray, float& out_t);
	static bool AABBAABB(const AABB& aabb1, const AABB& aabb2);
	static bool AABBSphere(const AABB& aabb, const Sphere& sphere);
	static bool AABBSegment(const AABB& aabb, const Segment& segment);
	static bool AABBLine(const AABB& aabb, const Line& line);
	static bool AABBRay(const AABB& aabb, const Ray& ray);
	static bool OBBSphere(const OBB& obb, const Sphere& sphere);
	static bool OBBSegment(const OBB& obb, const Segment& segment);
	static bool OBBLine(const OBB& obb, const Line& line);
	static bool OBBRay(const OBB& obb, const Ray& ray);
	static bool OBBOBB(const OBB& obb1, const OBB& obb2);

	static bool AABBLineBase(const AABB& aabb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax);
	static bool OBBLineBase(const OBB& obb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax);

private:
	static bool TriangleLineBase(const CollisionTriangle& triangle, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_t);
};
