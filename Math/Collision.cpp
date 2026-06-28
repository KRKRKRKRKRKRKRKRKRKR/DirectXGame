#include "Collision.h"

Vector3 Collision::Project(const Vector3& v1, const Vector3& v2) {
	float dot = VectorMath::Dot(v1, v2);
	float lengthSq = VectorMath::Dot(v2, v2);
	if (lengthSq == 0.0f) return { 0.0f, 0.0f, 0.0f };
	return v2 * (dot / lengthSq);
}

Vector3 Collision::ClosestPoint(const Vector3& point, const Segment& segment) {
	Vector3 a = point - segment.origin;
	float dot = VectorMath::Dot(a, segment.diff);
	float lengthSq = VectorMath::Dot(segment.diff, segment.diff);
	if (lengthSq == 0.0f) return segment.origin;
	float t = std::clamp(dot / lengthSq, 0.0f, 1.0f);
	return segment.origin + segment.diff * t;
}

OBB Collision::AABBToOBB(const AABB& aabb) {
	OBB obb;
	obb.center = (aabb.min + aabb.max) * 0.5f;  // center は min/max から計算
	obb.Orientation[0] = { 1.0f, 0.0f, 0.0f };
	obb.Orientation[1] = { 0.0f, 1.0f, 0.0f };
	obb.Orientation[2] = { 0.0f, 0.0f, 1.0f };
	obb.Size = (aabb.max - aabb.min) * 0.5f;
	return obb;
}

bool Collision::SphereSphere(const Sphere& s1, const Sphere& s2) {
	Vector3 d = s1.center - s2.center;
	float r = s1.radius + s2.radius;
	return VectorMath::Dot(d, d) <= r * r;
}

bool Collision::SpherePlane(const Sphere& sphere, const Plane& plane) {
	float dist = VectorMath::Dot(plane.normal, sphere.center) - plane.distance;
	return fabsf(dist) <= sphere.radius;
}

bool Collision::LinePlane(const Line& line, const Plane& plane, float& out_t, Vector3& out_intersection) {
	float dot = VectorMath::Dot(plane.normal, line.diff);
	if (fabsf(dot) < 1e-6f) return false;
	out_t = (plane.distance - VectorMath::Dot(plane.normal, line.origin)) / dot;
	out_intersection = line.origin + line.diff * out_t;
	return true;
}

bool Collision::SegmentPlane(const Segment& segment, const Plane& plane, float& out_t, Vector3& out_intersection) {
	float dot = VectorMath::Dot(plane.normal, segment.diff);
	if (fabsf(dot) < 1e-6f) return false;
	out_t = (plane.distance - VectorMath::Dot(plane.normal, segment.origin)) / dot;
	if (out_t < 0.0f || out_t > 1.0f) return false;
	out_intersection = segment.origin + segment.diff * out_t;
	return true;
}

bool Collision::RayPlane(const Ray& ray, const Plane& plane, float& out_t, Vector3& out_intersection) {
	float dot = VectorMath::Dot(plane.normal, ray.diff);
	if (fabsf(dot) < 1e-6f) return false;
	out_t = (plane.distance - VectorMath::Dot(plane.normal, ray.origin)) / dot;
	if (out_t < 0.0f) return false;
	out_intersection = ray.origin + ray.diff * out_t;
	return true;
}

bool Collision::TriangleLineBase(const CollisionTriangle& triangle, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_t) {
	Vector3 edge1 = triangle.vertices[1] - triangle.vertices[0];
	Vector3 edge2 = triangle.vertices[2] - triangle.vertices[0];
	Vector3 normal = VectorMath::Cross(edge1, edge2);
	float dot = VectorMath::Dot(normal, diff);
	if (fabsf(dot) < 1e-6f) return false;
	float t = (VectorMath::Dot(normal, triangle.vertices[0]) - VectorMath::Dot(normal, origin)) / dot;
	if (t < tMin || t > tMax) return false;
	Vector3 p = origin + diff * t;
	Vector3 c0 = VectorMath::Cross(edge1, p - triangle.vertices[0]);
	Vector3 c1 = VectorMath::Cross(triangle.vertices[2] - triangle.vertices[1], p - triangle.vertices[1]);
	Vector3 c2 = VectorMath::Cross(triangle.vertices[0] - triangle.vertices[2], p - triangle.vertices[2]);
	if (VectorMath::Dot(normal, c0) >= 0 && VectorMath::Dot(normal, c1) >= 0 && VectorMath::Dot(normal, c2) >= 0) {
		out_t = t;
		return true;
	}
	return false;
}

bool Collision::TriangleSegment(const CollisionTriangle& triangle, const Segment& segment, float& out_t) {
	return TriangleLineBase(triangle, segment.origin, segment.diff, 0.0f, 1.0f, out_t);
}

bool Collision::TriangleLine(const CollisionTriangle& triangle, const Line& line, float& out_t) {
	return TriangleLineBase(triangle, line.origin, line.diff, -FLT_MAX, FLT_MAX, out_t);
}

bool Collision::TriangleRay(const CollisionTriangle& triangle, const Ray& ray, float& out_t) {
	return TriangleLineBase(triangle, ray.origin, ray.diff, 0.0f, FLT_MAX, out_t);
}

bool Collision::AABBAABB(const AABB& aabb1, const AABB& aabb2) {
	return (aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) &&
	       (aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) &&
	       (aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z);
}

bool Collision::AABBSphere(const AABB& aabb, const Sphere& sphere) {
	Vector3 closest = {
		std::clamp(sphere.center.x, aabb.min.x, aabb.max.x),
		std::clamp(sphere.center.y, aabb.min.y, aabb.max.y),
		std::clamp(sphere.center.z, aabb.min.z, aabb.max.z),
	};
	Vector3 d = closest - sphere.center;
	return VectorMath::Dot(d, d) <= sphere.radius * sphere.radius;
}

bool Collision::AABBLineBase(const AABB& aabb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax) {
	float tMin_ = tMin;
	float tMax_ = tMax;

	const float* minV = &aabb.min.x;
	const float* maxV = &aabb.max.x;
	const float* orig = &origin.x;
	const float* dir  = &diff.x;

	for (int i = 0; i < 3; i++) {
		if (std::abs(dir[i]) < 1e-6f) {
			if (orig[i] < minV[i] || orig[i] > maxV[i]) return false;
		} else {
			float t1 = (minV[i] - orig[i]) / dir[i];
			float t2 = (maxV[i] - orig[i]) / dir[i];
			if (t1 > t2) std::swap(t1, t2);
			tMin_ = (std::max)(tMin_, t1);
			tMax_ = (std::min)(tMax_, t2);
			if (tMin_ > tMax_) return false;
		}
	}

	out_tMin = tMin_;
	out_tMax = tMax_;
	return true;
}

bool Collision::AABBSegment(const AABB& aabb, const Segment& segment) {
	float tMin, tMax;
	return AABBLineBase(aabb, segment.origin, segment.diff, 0.0f, 1.0f, tMin, tMax);
}

bool Collision::AABBLine(const AABB& aabb, const Line& line) {
	float tMin, tMax;
	return AABBLineBase(aabb, line.origin, line.diff, -FLT_MAX, FLT_MAX, tMin, tMax);
}

bool Collision::AABBRay(const AABB& aabb, const Ray& ray) {
	float tMin, tMax;
	return AABBLineBase(aabb, ray.origin, ray.diff, 0.0f, FLT_MAX, tMin, tMax);
}

bool Collision::OBBSphere(const OBB& obb, const Sphere& sphere) {
	Vector3 diff = sphere.center - obb.center;
	Vector3 local = {
		VectorMath::Dot(diff, obb.Orientation[0]),
		VectorMath::Dot(diff, obb.Orientation[1]),
		VectorMath::Dot(diff, obb.Orientation[2]),
	};
	Vector3 closest = {
		std::clamp(local.x, -obb.Size.x, obb.Size.x),
		std::clamp(local.y, -obb.Size.y, obb.Size.y),
		std::clamp(local.z, -obb.Size.z, obb.Size.z),
	};
	Vector3 d = local - closest;
	return VectorMath::Dot(d, d) <= sphere.radius * sphere.radius;
}

bool Collision::OBBLineBase(const OBB& obb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax) {
	Vector3 d = origin - obb.center;
	Vector3 localOrigin = {
		VectorMath::Dot(d, obb.Orientation[0]),
		VectorMath::Dot(d, obb.Orientation[1]),
		VectorMath::Dot(d, obb.Orientation[2]),
	};
	Vector3 localDiff = {
		VectorMath::Dot(diff, obb.Orientation[0]),
		VectorMath::Dot(diff, obb.Orientation[1]),
		VectorMath::Dot(diff, obb.Orientation[2]),
	};
	AABB localAABB;
	localAABB.min = { -obb.Size.x, -obb.Size.y, -obb.Size.z };
	localAABB.max = obb.Size;
	return AABBLineBase(localAABB, localOrigin, localDiff, tMin, tMax, out_tMin, out_tMax);
}

bool Collision::OBBSegment(const OBB& obb, const Segment& segment) {
	float tMin, tMax;
	return OBBLineBase(obb, segment.origin, segment.diff, 0.0f, 1.0f, tMin, tMax);
}

bool Collision::OBBLine(const OBB& obb, const Line& line) {
	float tMin, tMax;
	return OBBLineBase(obb, line.origin, line.diff, -FLT_MAX, FLT_MAX, tMin, tMax);
}

bool Collision::OBBRay(const OBB& obb, const Ray& ray) {
	float tMin, tMax;
	return OBBLineBase(obb, ray.origin, ray.diff, 0.0f, FLT_MAX, tMin, tMax);
}

bool Collision::OBBOBB(const OBB& obb1, const OBB& obb2) {
	Vector3 d = obb2.center - obb1.center;

	Vector3 axes[15];
	axes[0]  = obb1.Orientation[0];
	axes[1]  = obb1.Orientation[1];
	axes[2]  = obb1.Orientation[2];
	axes[3]  = obb2.Orientation[0];
	axes[4]  = obb2.Orientation[1];
	axes[5]  = obb2.Orientation[2];
	axes[6]  = VectorMath::Cross(obb1.Orientation[0], obb2.Orientation[0]);
	axes[7]  = VectorMath::Cross(obb1.Orientation[0], obb2.Orientation[1]);
	axes[8]  = VectorMath::Cross(obb1.Orientation[0], obb2.Orientation[2]);
	axes[9]  = VectorMath::Cross(obb1.Orientation[1], obb2.Orientation[0]);
	axes[10] = VectorMath::Cross(obb1.Orientation[1], obb2.Orientation[1]);
	axes[11] = VectorMath::Cross(obb1.Orientation[1], obb2.Orientation[2]);
	axes[12] = VectorMath::Cross(obb1.Orientation[2], obb2.Orientation[0]);
	axes[13] = VectorMath::Cross(obb1.Orientation[2], obb2.Orientation[1]);
	axes[14] = VectorMath::Cross(obb1.Orientation[2], obb2.Orientation[2]);

	for (int i = 0; i < 15; i++) {
		if (VectorMath::Dot(axes[i], axes[i]) < 1e-6f) continue;
		float r1 =
			fabsf(VectorMath::Dot(obb1.Orientation[0] * obb1.Size.x, axes[i])) +
			fabsf(VectorMath::Dot(obb1.Orientation[1] * obb1.Size.y, axes[i])) +
			fabsf(VectorMath::Dot(obb1.Orientation[2] * obb1.Size.z, axes[i]));
		float r2 =
			fabsf(VectorMath::Dot(obb2.Orientation[0] * obb2.Size.x, axes[i])) +
			fabsf(VectorMath::Dot(obb2.Orientation[1] * obb2.Size.y, axes[i])) +
			fabsf(VectorMath::Dot(obb2.Orientation[2] * obb2.Size.z, axes[i]));
		if (fabsf(VectorMath::Dot(d, axes[i])) > r1 + r2) return false;
	}

	return true;
}
