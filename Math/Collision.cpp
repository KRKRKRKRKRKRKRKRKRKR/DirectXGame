#include "Collision.h"

namespace Collision {

Vector3 Project(const Vector3& v1, const Vector3& v2) {
	float dot = VectorMath::Dot(v1, v2);
	float lengthSq = VectorMath::Dot(v2, v2);
	if (lengthSq == 0.0f) return { 0.0f, 0.0f, 0.0f };
	return v2 * (dot / lengthSq);
}

Vector3 ClosestPoint(const Vector3& point, const Segment& segment) {
	Vector3 a = point - segment.origin;
	float dot = VectorMath::Dot(a, segment.diff);
	float lengthSq = VectorMath::Dot(segment.diff, segment.diff);
	if (lengthSq == 0.0f) return segment.origin;
	float t = std::clamp(dot / lengthSq, 0.0f, 1.0f);
	return segment.origin + segment.diff * t;
}

OBB AABBToOBB(const AABB& aabb) {
	OBB obb;
	obb.center = (aabb.min + aabb.max) * 0.5f;  // center は min/max から計算
	obb.Orientation[0] = { 1.0f, 0.0f, 0.0f };
	obb.Orientation[1] = { 0.0f, 1.0f, 0.0f };
	obb.Orientation[2] = { 0.0f, 0.0f, 1.0f };
	obb.Size = (aabb.max - aabb.min) * 0.5f;
	return obb;
}

bool SphereSphere(const Sphere& s1, const Sphere& s2) {
	Vector3 d = s1.center - s2.center;
	float r = s1.radius + s2.radius;
	return VectorMath::Dot(d, d) <= r * r;
}

bool SpherePlane(const Sphere& sphere, const Plane& plane) {
	float dist = VectorMath::Dot(plane.normal, sphere.center) - plane.distance;
	return fabsf(dist) <= sphere.radius;
}

bool RaySphere(const Ray& ray, const Sphere& sphere, float& out_t) {
	Vector3 m = ray.origin - sphere.center;
	float b = VectorMath::Dot(m, ray.diff);
	float c = VectorMath::Dot(m, m) - sphere.radius * sphere.radius;
	if (c > 0.0f && b > 0.0f) return false; // 原点が球の外側にあり、レイが球から遠ざかる方向
	float diffLenSq = VectorMath::Dot(ray.diff, ray.diff);
	if (diffLenSq < 1e-12f) return false;
	float discriminant = b * b - diffLenSq * c;
	if (discriminant < 0.0f) return false;
	float t = (-b - sqrtf(discriminant)) / diffLenSq;
	if (t < 0.0f) t = 0.0f; // レイ原点が既に球の内側にある場合
	out_t = t;
	return true;
}

namespace {
	// (a,b,c,d)から法線(a,b,c)を単位長に正規化し、dも同じ係数でスケールする
	// （距離判定 dot(normal,point)+d の単位を球のradiusと揃えるために必須）
	Plane NormalizePlane(float a, float b, float c, float d) {
		float len = sqrtf(a * a + b * b + c * c);
		if (len < 1e-8f) len = 1e-8f;
		return Plane{ Vector3{ a / len, b / len, c / len }, d / len };
	}

	// OBBOBBのSAT判定で調べる15本の分離軸候補（各OBBの3軸＋それぞれの外積9本）を組み立てる。
	// OBBOBB/OBBOBBPenetrationの両方から使う共通部分をここに集約する
	void BuildOBBAxes15(const OBB& obb1, const OBB& obb2, Vector3 axes[15]) {
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
	}
}

Frustum MakeFrustumFromViewProjection(const Matrix4x4& viewProjection) {
	const Matrix4x4& m = viewProjection;
	Frustum f;
	// 行ベクトル規約(v'=v*M)のGribb-Hartmann法。距離側の符号は
	// Plane.distance を「dot(normal,point) - distance」で使う既存SpherePlaneの規約に合わせるため反転させている
	f.planes[0] = NormalizePlane(m.m[0][3] + m.m[0][0], m.m[1][3] + m.m[1][0], m.m[2][3] + m.m[2][0], -(m.m[3][3] + m.m[3][0])); // left
	f.planes[1] = NormalizePlane(m.m[0][3] - m.m[0][0], m.m[1][3] - m.m[1][0], m.m[2][3] - m.m[2][0], -(m.m[3][3] - m.m[3][0])); // right
	f.planes[2] = NormalizePlane(m.m[0][3] + m.m[0][1], m.m[1][3] + m.m[1][1], m.m[2][3] + m.m[2][1], -(m.m[3][3] + m.m[3][1])); // bottom
	f.planes[3] = NormalizePlane(m.m[0][3] - m.m[0][1], m.m[1][3] - m.m[1][1], m.m[2][3] - m.m[2][1], -(m.m[3][3] - m.m[3][1])); // top
	f.planes[4] = NormalizePlane(m.m[0][2],              m.m[1][2],              m.m[2][2],              -(m.m[3][2]));              // near (LH, 0〜1深度)
	f.planes[5] = NormalizePlane(m.m[0][3] - m.m[0][2], m.m[1][3] - m.m[1][2], m.m[2][3] - m.m[2][2], -(m.m[3][3] - m.m[3][2])); // far
	return f;
}

bool SphereFrustum(const Sphere& sphere, const Frustum& frustum) {
	for (const Plane& plane : frustum.planes) {
		float dist = VectorMath::Dot(plane.normal, sphere.center) - plane.distance;
		// distが負に大きい（-radiusより小さい）＝球全体がこの平面の外側 → 完全にカリングできる
		if (dist < -sphere.radius) return false;
	}
	return true;
}

bool LinePlane(const Line& line, const Plane& plane, float& out_t, Vector3& out_intersection) {
	float dot = VectorMath::Dot(plane.normal, line.diff);
	if (fabsf(dot) < 1e-6f) return false;
	out_t = (plane.distance - VectorMath::Dot(plane.normal, line.origin)) / dot;
	out_intersection = line.origin + line.diff * out_t;
	return true;
}

bool SegmentPlane(const Segment& segment, const Plane& plane, float& out_t, Vector3& out_intersection) {
	float dot = VectorMath::Dot(plane.normal, segment.diff);
	if (fabsf(dot) < 1e-6f) return false;
	out_t = (plane.distance - VectorMath::Dot(plane.normal, segment.origin)) / dot;
	if (out_t < 0.0f || out_t > 1.0f) return false;
	out_intersection = segment.origin + segment.diff * out_t;
	return true;
}

bool RayPlane(const Ray& ray, const Plane& plane, float& out_t, Vector3& out_intersection) {
	float dot = VectorMath::Dot(plane.normal, ray.diff);
	if (fabsf(dot) < 1e-6f) return false;
	out_t = (plane.distance - VectorMath::Dot(plane.normal, ray.origin)) / dot;
	if (out_t < 0.0f) return false;
	out_intersection = ray.origin + ray.diff * out_t;
	return true;
}

bool TriangleLineBase(const CollisionTriangle& triangle, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_t) {
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

bool TriangleSegment(const CollisionTriangle& triangle, const Segment& segment, float& out_t) {
	return TriangleLineBase(triangle, segment.origin, segment.diff, 0.0f, 1.0f, out_t);
}

bool TriangleLine(const CollisionTriangle& triangle, const Line& line, float& out_t) {
	return TriangleLineBase(triangle, line.origin, line.diff, -FLT_MAX, FLT_MAX, out_t);
}

bool TriangleRay(const CollisionTriangle& triangle, const Ray& ray, float& out_t) {
	return TriangleLineBase(triangle, ray.origin, ray.diff, 0.0f, FLT_MAX, out_t);
}

bool AABBAABB(const AABB& aabb1, const AABB& aabb2) {
	return (aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) &&
	       (aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) &&
	       (aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z);
}

bool AABBSphere(const AABB& aabb, const Sphere& sphere) {
	Vector3 closest = {
		std::clamp(sphere.center.x, aabb.min.x, aabb.max.x),
		std::clamp(sphere.center.y, aabb.min.y, aabb.max.y),
		std::clamp(sphere.center.z, aabb.min.z, aabb.max.z),
	};
	Vector3 d = closest - sphere.center;
	return VectorMath::Dot(d, d) <= sphere.radius * sphere.radius;
}

bool AABBLineBase(const AABB& aabb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax) {
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

bool AABBSegment(const AABB& aabb, const Segment& segment) {
	float tMin, tMax;
	return AABBLineBase(aabb, segment.origin, segment.diff, 0.0f, 1.0f, tMin, tMax);
}

bool AABBLine(const AABB& aabb, const Line& line) {
	float tMin, tMax;
	return AABBLineBase(aabb, line.origin, line.diff, -FLT_MAX, FLT_MAX, tMin, tMax);
}

bool AABBRay(const AABB& aabb, const Ray& ray) {
	float tMin, tMax;
	return AABBLineBase(aabb, ray.origin, ray.diff, 0.0f, FLT_MAX, tMin, tMax);
}

bool OBBSphere(const OBB& obb, const Sphere& sphere) {
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

bool SegmentSphere(const Segment& segment, const Sphere& sphere) {
	Vector3 closest = ClosestPoint(sphere.center, segment);
	Vector3 d = sphere.center - closest;
	return VectorMath::Dot(d, d) <= sphere.radius * sphere.radius;
}

bool OBBLineBase(const OBB& obb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax) {
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

bool OBBSegment(const OBB& obb, const Segment& segment) {
	float tMin, tMax;
	return OBBLineBase(obb, segment.origin, segment.diff, 0.0f, 1.0f, tMin, tMax);
}

bool OBBLine(const OBB& obb, const Line& line) {
	float tMin, tMax;
	return OBBLineBase(obb, line.origin, line.diff, -FLT_MAX, FLT_MAX, tMin, tMax);
}

bool OBBRay(const OBB& obb, const Ray& ray) {
	float tMin, tMax;
	return OBBLineBase(obb, ray.origin, ray.diff, 0.0f, FLT_MAX, tMin, tMax);
}

bool OBBOBB(const OBB& obb1, const OBB& obb2) {
	Vector3 d = obb2.center - obb1.center;

	Vector3 axes[15];
	BuildOBBAxes15(obb1, obb2, axes);

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

bool SphereSpherePenetration(const Sphere& a, const Sphere& b, Vector3& out_normal, float& out_depth) {
	Vector3 d = b.center - a.center; // a→b向き
	float dist = VectorMath::Length(d);
	float r = a.radius + b.radius;
	if (dist > r) return false;

	out_depth = r - dist;
	// 中心が完全一致する退化ケース（distがほぼ0で向きを決められない）は上向きにフォールバックする
	out_normal = (dist > 1e-6f) ? d * (1.0f / dist) : Vector3{ 0.0f, 1.0f, 0.0f };
	return true;
}

bool OBBOBBPenetration(const OBB& obb1, const OBB& obb2, Vector3& out_normal, float& out_depth) {
	Vector3 d = obb2.center - obb1.center; // obb1→obb2向き

	Vector3 axes[15];
	BuildOBBAxes15(obb1, obb2, axes);

	float minOverlap = FLT_MAX;
	Vector3 minAxis{ 0.0f, 0.0f, 0.0f };
	bool found = false;

	for (int i = 0; i < 15; i++) {
		float lenSq = VectorMath::Dot(axes[i], axes[i]);
		if (lenSq < 1e-6f) continue; // 2軸がほぼ平行で外積が退化している場合はスキップ（OBBOBBと同様）

		// MTVは実距離（重なりの深さ）として意味を持たせる必要があるため、SAT本体と違い軸を正規化する
		Vector3 axis = axes[i] * (1.0f / sqrtf(lenSq));
		float r1 =
			fabsf(VectorMath::Dot(obb1.Orientation[0] * obb1.Size.x, axis)) +
			fabsf(VectorMath::Dot(obb1.Orientation[1] * obb1.Size.y, axis)) +
			fabsf(VectorMath::Dot(obb1.Orientation[2] * obb1.Size.z, axis));
		float r2 =
			fabsf(VectorMath::Dot(obb2.Orientation[0] * obb2.Size.x, axis)) +
			fabsf(VectorMath::Dot(obb2.Orientation[1] * obb2.Size.y, axis)) +
			fabsf(VectorMath::Dot(obb2.Orientation[2] * obb2.Size.z, axis));
		float overlap = (r1 + r2) - fabsf(VectorMath::Dot(d, axis));
		if (overlap < 0.0f) return false; // 分離軸が見つかった＝重なっていない

		if (overlap < minOverlap) {
			minOverlap = overlap;
			minAxis = axis;
			found = true;
		}
	}
	if (!found) return false; // 全軸が退化していた場合（実質起こりえないが念のため）

	// minAxisはobb1→obb2向きになるよう符号を揃える（SATでは軸の向きは任意のため）
	if (VectorMath::Dot(d, minAxis) < 0.0f) minAxis = minAxis * -1.0f;

	out_normal = minAxis;
	out_depth  = minOverlap;
	return true;
}

bool OBBSpherePenetration(const OBB& obb, const Sphere& sphere, Vector3& out_normal, float& out_depth) {
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
	Vector3 dLocal = local - closest;
	float distSq = VectorMath::Dot(dLocal, dLocal);
	if (distSq > sphere.radius * sphere.radius) return false;

	Vector3 normalLocal;
	if (distSq > 1e-8f) {
		// 通常ケース：球の中心がOBBの外側（面上含む）にある。最近接点→球中心の向きがそのまま押し出し方向
		float dist = sqrtf(distSq);
		normalLocal = dLocal * (1.0f / dist);
		out_depth = sphere.radius - dist;
	} else {
		// 退化ケース：球の中心がOBBの内部にある（最近接点=中心そのもので方向が決まらない）。
		// 中心から各面までの最短距離を比較し、最も近い面の向きに押し出す
		float penX = obb.Size.x - fabsf(local.x);
		float penY = obb.Size.y - fabsf(local.y);
		float penZ = obb.Size.z - fabsf(local.z);
		float faceDepth;
		if (penX <= penY && penX <= penZ) {
			normalLocal = { local.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
			faceDepth = penX;
		} else if (penY <= penZ) {
			normalLocal = { 0.0f, local.y >= 0.0f ? 1.0f : -1.0f, 0.0f };
			faceDepth = penY;
		} else {
			normalLocal = { 0.0f, 0.0f, local.z >= 0.0f ? 1.0f : -1.0f };
			faceDepth = penZ;
		}
		out_depth = faceDepth + sphere.radius; // 面から球中心までの距離ぶんに加え、半径ぶんも押し出す
	}

	// ローカル軸の法線をOBBのワールド軸に戻す（obb→sphere向き）
	out_normal = obb.Orientation[0] * normalLocal.x + obb.Orientation[1] * normalLocal.y + obb.Orientation[2] * normalLocal.z;
	return true;
}

} // namespace Collision
