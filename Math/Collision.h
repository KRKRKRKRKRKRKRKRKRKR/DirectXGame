#pragma once
#include "MathTypes.h"
#include "VectorMath.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

// 当たり判定専用の型・関数群。描画オブジェクトのSphere/Line等と名前が衝突しないよう
// namespace Collisionで囲んでいる（呼び出し側は Collision::Sphere のように修飾する）
namespace Collision {

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

// 視錐台の6平面（左右上下近遠）。各平面の法線は錐台の内側を向く
struct Frustum {
	Plane planes[6];
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

Vector3 Project(const Vector3& v1, const Vector3& v2);
Vector3 ClosestPoint(const Vector3& point, const Segment& segment);
OBB     AABBToOBB(const AABB& aabb);

bool SphereSphere(const Sphere& s1, const Sphere& s2);
bool SpherePlane(const Sphere& sphere, const Plane& plane);

// view*projection行列（DirectXMath流儀：行ベクトル・右からかける規約）から視錐台の6平面を抽出する
Frustum MakeFrustumFromViewProjection(const Matrix4x4& viewProjection);
// フラストラムカリング用。球が6平面すべての内側（またはまたがる）にあればtrue、
// いずれかの平面に完全に外側であればfalse（描画不要と判定できる）
bool SphereFrustum(const Sphere& sphere, const Frustum& frustum);
bool LinePlane(const Line& line, const Plane& plane, float& out_t, Vector3& out_intersection);
bool SegmentPlane(const Segment& segment, const Plane& plane, float& out_t, Vector3& out_intersection);
bool RayPlane(const Ray& ray, const Plane& plane, float& out_t, Vector3& out_intersection);
bool TriangleSegment(const CollisionTriangle& triangle, const Segment& segment, float& out_t);
bool TriangleLine(const CollisionTriangle& triangle, const Line& line, float& out_t);
bool TriangleRay(const CollisionTriangle& triangle, const Ray& ray, float& out_t);
bool AABBAABB(const AABB& aabb1, const AABB& aabb2);
bool AABBSphere(const AABB& aabb, const Sphere& sphere);
bool AABBSegment(const AABB& aabb, const Segment& segment);
bool AABBLine(const AABB& aabb, const Line& line);
bool AABBRay(const AABB& aabb, const Ray& ray);
bool OBBSphere(const OBB& obb, const Sphere& sphere);
bool OBBSegment(const OBB& obb, const Segment& segment);
bool OBBLine(const OBB& obb, const Line& line);
bool OBBRay(const OBB& obb, const Ray& ray);
bool OBBOBB(const OBB& obb1, const OBB& obb2);

bool AABBLineBase(const AABB& aabb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax);
bool OBBLineBase(const OBB& obb, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_tMin, float& out_tMax);

// 内部実装専用（他の翻訳単位から呼ぶ想定はないが、名前空間内なので呼べてしまう点に注意）
bool TriangleLineBase(const CollisionTriangle& triangle, const Vector3& origin, const Vector3& diff, float tMin, float tMax, float& out_t);

} // namespace Collision
