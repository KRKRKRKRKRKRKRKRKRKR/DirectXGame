#include "ColliderSystem.h"
#include "../GameObject.h"
#include "../Component/Physics/ColliderComponentBase.h"
#include "../Component/Physics/SphereColliderComponent.h"
#include "../Component/Physics/OBBColliderComponent.h"
#include "../Component/Physics/GravityComponent.h"
#include "../../../Math/Collision.h"
#include "../../Graphics/Renderer/Renderer.h"

void ColliderSystem::ResolveAndDraw(const std::vector<GameObject*>& targets, bool isPlaying,
	Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj) {
	constexpr Vector4 kNoOverlapColor      = { 0.2f, 1.0f, 0.2f, 1.0f }; // 緑：重なりなし
	constexpr Vector4 kTriggerOverlapColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄：Trigger込みの重なり
	constexpr Vector4 kSolidOverlapColor   = { 1.0f, 0.2f, 0.2f, 1.0f }; // 赤：Solid同士の重なり

	// 全オブジェクトのColliderを先に集めておく（N×Nの重なり判定を1回で済ませるため）
	struct Entry {
		GameObject* obj;
		SphereColliderComponent* sphere;
		OBBColliderComponent* obb;
		ColliderComponentBase* base; // layer/isTrigger参照用。sphere/obbのどちらか一方を指す
		bool overlappingSolid   = false;
		bool overlappingTrigger = false;
	};
	std::vector<Entry> entries;
	for (auto* obj : targets) {
		auto* sphere = obj->GetComponent<SphereColliderComponent>();
		auto* obb    = obj->GetComponent<OBBColliderComponent>();
		ColliderComponentBase* base = obj->GetComponent<ColliderComponentBase>();
		if (base) entries.push_back({ obj, sphere, obb, base });
	}

	// 総当たりで重なりを判定（オブジェクト数が少ないため計算量は無視できる）
	for (size_t i = 0; i < entries.size(); i++) {
		for (size_t j = i + 1; j < entries.size(); j++) {
			auto& a = entries[i];
			auto& b = entries[j];

			// レイヤーの組み合わせが判定対象外なら幾何計算自体をスキップする
			// （「Obstacle同士は判定しない」等をここ1行で表現できる）
			if (!ShouldLayersCollide(*a.base, *b.base)) continue;

			bool hit = false;
			if (a.sphere && b.sphere) {
				hit = Collision::SphereSphere(a.sphere->GetWorldSphere(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()));
			} else if (a.obb && b.obb) {
				hit = Collision::OBBOBB(a.obb->GetWorldOBB(a.obj->GetTransform()), b.obb->GetWorldOBB(b.obj->GetTransform()));
			} else if (a.sphere && b.obb) {
				hit = Collision::OBBSphere(b.obb->GetWorldOBB(b.obj->GetTransform()), a.sphere->GetWorldSphere(a.obj->GetTransform()));
			} else if (a.obb && b.sphere) {
				hit = Collision::OBBSphere(a.obb->GetWorldOBB(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()));
			}
			if (!hit) continue;

			// 片方でもTriggerならこのペアはTrigger重なり、両方Solidの場合のみSolid重なり
			if (a.base->isTrigger || b.base->isTrigger) {
				a.overlappingTrigger = true; b.overlappingTrigger = true;
			} else {
				a.overlappingSolid = true; b.overlappingSolid = true;

				// Stop中（isPlaying=false）は押し戻しを行わない。重なりの検知・色分け表示だけは
				// 常時行い、Gizmoでの自由な配置を妨げないようにする
				if (!isPlaying) continue;

				// 両方Solid：実際に押し戻して重なりを解消する（半分ずつ均等に分配）
				Vector3 normal{ 0.0f, 0.0f, 0.0f }; // a→b向き
				float   depth = 0.0f;
				bool    resolved = false;
				if (a.sphere && b.sphere) {
					resolved = Collision::SphereSpherePenetration(a.sphere->GetWorldSphere(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()), normal, depth);
				} else if (a.obb && b.obb) {
					resolved = Collision::OBBOBBPenetration(a.obb->GetWorldOBB(a.obj->GetTransform()), b.obb->GetWorldOBB(b.obj->GetTransform()), normal, depth);
				} else if (a.sphere && b.obb) {
					// OBBSpherePenetrationはobb→sphere向きを返すため、a(sphere)→b(obb)向きに揃えるため反転する
					resolved = Collision::OBBSpherePenetration(b.obb->GetWorldOBB(b.obj->GetTransform()), a.sphere->GetWorldSphere(a.obj->GetTransform()), normal, depth);
					normal = normal * -1.0f;
				} else if (a.obb && b.sphere) {
					resolved = Collision::OBBSpherePenetration(a.obb->GetWorldOBB(a.obj->GetTransform()), b.sphere->GetWorldSphere(b.obj->GetTransform()), normal, depth);
				}
				if (resolved) {
					// isStaticな側は押し戻しで動かさない。両方staticなら押し戻し自体をスキップし、
					// 片方だけstaticならもう片方に100%を割り当てる（デフォルトは従来通り50/50）
					float aRatio = 0.5f, bRatio = 0.5f;
					if (a.base->isStatic && !b.base->isStatic) {
						aRatio = 0.0f; bRatio = 1.0f;
					} else if (b.base->isStatic && !a.base->isStatic) {
						aRatio = 1.0f; bRatio = 0.0f;
					} else if (a.base->isStatic && b.base->isStatic) {
						aRatio = 0.0f; bRatio = 0.0f;
					}

					Vector3 aDelta = normal * (-depth * aRatio);
					Vector3 bDelta = normal * (depth * bRatio);
					a.obj->GetTransform().translation = a.obj->GetTransform().translation + aDelta;
					b.obj->GetTransform().translation = b.obj->GetTransform().translation + bDelta;

					// 重力で落下中のオブジェクトが押し戻しで上方向に着地した場合、蓄積された落下速度を
					// リセットする（リセットしないと同じ速度で再びめり込み、押し戻しが毎フレーム
					// 発生して振動してしまう）
					if (GravityComponent* aGravity = a.obj->GetComponent<GravityComponent>()) {
						if (aDelta.y > 0.0f && aGravity->velocityY < 0.0f) aGravity->velocityY = 0.0f;
					}
					if (GravityComponent* bGravity = b.obj->GetComponent<GravityComponent>()) {
						if (bDelta.y > 0.0f && bGravity->velocityY < 0.0f) bGravity->velocityY = 0.0f;
					}
				}
			}
		}
	}

	// 描画は各コンポーネント自身のDrawWireframeに委譲する（ColliderSystemは判定結果の色だけ渡す）。
	// 1つのオブジェクトが複数と同時に重なる場合はSolid重なりを優先して赤にする
	// （「ぶつかって止まる」方が「すり抜けて拾う」より目視上の優先度が高いため）
	for (auto& e : entries) {
		Vector4 color = e.overlappingSolid   ? kSolidOverlapColor
		              : e.overlappingTrigger ? kTriggerOverlapColor
		                                     : kNoOverlapColor;
		if (e.sphere) e.sphere->DrawWireframe(renderer, e.obj->GetTransform(), color, view, proj);
		if (e.obb)    e.obb->DrawWireframe(renderer, e.obj->GetTransform(), color, view, proj);
	}
}
