#include "ColliderSystem.h"
#include "../GameObject.h"
#include "../Component/Physics/ColliderComponentBase.h"
#include "../Component/Physics/SphereColliderComponent.h"
#include "../Component/Physics/OBBColliderComponent.h"
#include "../Component/Physics/GravityComponent.h"
#include "../Component/Physics/GravityFlipComponent.h"
#include "../../../Math/Collision.h"
#include "../../../Math/VectorMath.h"
#include "../../Graphics/Renderer/Renderer.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {
// 総当たり判定（O(N²)）が敵200体規模で目に見えて重くなったため、均一グリッドによる
// ブロードフェーズを挟む。このゲームのフィールドはX-Y平面（Z=0固定、範囲は概ね±10程度）に
// 収まるため、2次元グリッドで十分（3次元対応は今のところ不要）。
// セルサイズはコライダーの想定サイズ（半径1前後）より一回り大きい値にしておくと、
// 1オブジェクトが跨るセル数が増えすぎず、かつ近傍セルの取りこぼしが起きにくい
constexpr float kGridCellSize = 4.0f;

int64_t CellKey(int cx, int cy) {
	// 2つのint32をint64に詰めるだけの単純なハッシュキー（負値を考慮してオフセットする）
	constexpr int64_t kOffset = 1 << 20;
	return (static_cast<int64_t>(cx + kOffset) << 32) | static_cast<int64_t>(cy + kOffset);
}

int CellCoord(float value) {
	return static_cast<int>(std::floor(value / kGridCellSize));
}
}

void ColliderSystem::ResolveAndDraw(const std::vector<GameObject*>& targets, bool isPlaying,
	Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj, bool drawDebug) {
	constexpr Vector4 kNoOverlapColor      = { 0.2f, 1.0f, 0.2f, 1.0f }; // 緑：重なりなし
	constexpr Vector4 kTriggerOverlapColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄：Trigger込みの重なり
	constexpr Vector4 kSolidOverlapColor   = { 1.0f, 0.2f, 0.2f, 1.0f }; // 赤：Solid同士の重なり

	// 全オブジェクトのColliderを先に集めておく（N×Nの重なり判定を1回で済ませるため）。
	// worldTransform/broadphaseRadiusはこの後のグリッド構築・ペア判定の両方で使うため、
	// GetWorldTransform()の呼び出し自体をペアごとではなくオブジェクトごとに1回だけに抑える
	struct Entry {
		GameObject* obj;
		SphereColliderComponent* sphere;
		OBBColliderComponent* obb;
		ColliderComponentBase* base; // layer/isTrigger参照用。sphere/obbのどちらか一方を指す
		Transform worldTransform;
		float broadphaseRadius = 0.0f; // グリッドの近傍セル判定に使う概算半径（球=radius、OBB=Sizeの最大成分）
		bool overlappingSolid   = false;
		bool overlappingTrigger = false;
		// 今フレームTrigger重なりだった相手の一覧。ループの最後にbase->triggerOverlapsLastFrame_へ
		// 書き戻し、次フレームの「新規侵入か」判定（OnTriggerEnter発火）に使う
		std::vector<GameObject*> triggerOverlapsThisFrame;
	};
	std::vector<Entry> entries;
	for (auto* obj : targets) {
		auto* sphere = obj->GetComponent<SphereColliderComponent>();
		auto* obb    = obj->GetComponent<OBBColliderComponent>();
		ColliderComponentBase* base = obj->GetComponent<ColliderComponentBase>();
		if (!base) continue;

		Entry entry{ obj, sphere, obb, base };
		entry.worldTransform = obj->GetWorldTransform();
		if (sphere) {
			entry.broadphaseRadius = sphere->GetWorldSphere(entry.worldTransform).radius;
		} else if (obb) {
			Vector3 size = obb->GetWorldOBB(entry.worldTransform).Size;
			entry.broadphaseRadius = (std::max)({ size.x, size.y, size.z });
		}
		entries.push_back(std::move(entry));
	}

	// 均一グリッドへ登録する（ブロードフェーズ）。X-Y平面のみ見る2次元グリッド
	// （フィールドがZ=0固定のため、Zを無視しても実害がない設計に合わせている）。
	// ただし壁（TopWall等）のようにセルサイズを大きく超える巨大コライダーは3x3近傍セルでは
	// 取りこぼしてしまう（壁の端から離れた位置にいる敵が、壁の中心が属するセルの近傍に
	// 入らず判定漏れする）ため、グリッドには入れず「常に全員の近傍候補」という別リストに回す
	std::unordered_map<int64_t, std::vector<size_t>> grid;
	std::vector<size_t> oversizedEntries; // broadphaseRadiusがセルサイズを超えるもの（壁等）
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].broadphaseRadius > kGridCellSize) {
			oversizedEntries.push_back(i);
			continue;
		}
		const Vector3& pos = entries[i].worldTransform.translation;
		grid[CellKey(CellCoord(pos.x), CellCoord(pos.y))].push_back(i);
	}

	// 近傍セル（自セル＋周囲8セルの計9セル）に属するインデックスを候補として集める
	// （oversizedEntriesとの判定はこの関数の外で別途・無条件に行うためここには含めない）
	auto collectNearby = [&](const Entry& e) {
		std::vector<size_t> result;
		int cx = CellCoord(e.worldTransform.translation.x);
		int cy = CellCoord(e.worldTransform.translation.y);
		for (int dx = -1; dx <= 1; dx++) {
			for (int dy = -1; dy <= 1; dy++) {
				auto it = grid.find(CellKey(cx + dx, cy + dy));
				if (it == grid.end()) continue;
				result.insert(result.end(), it->second.begin(), it->second.end());
			}
		}
		return result;
	};

	// ペアを重複なく1回ずつ処理する。通常のグリッド近傍同士はi<jの片方向のみ、
	// oversized（壁等）とグリッド内エントリとの組は、oversized側がi<jの順序に関係なく
	// 全グリッドエントリの近傍候補になりうるため、oversizedEntries自体を「常に相手側」とみなし
	// 無条件で1回だけ判定する（oversized同士はShouldLayersCollideがObstacle同士を弾くため
	// 実質判定されない）
	auto resolvePair = [&](size_t i, size_t j) {
		auto& a = entries[i];
		auto& b = entries[j];

		// レイヤーの組み合わせが判定対象外なら幾何計算自体をスキップする
		// （「Obstacle同士は判定しない」等をここ1行で表現できる）
		if (!ShouldLayersCollide(*a.base, *b.base)) return;

		const Transform& aWorld = a.worldTransform;
		const Transform& bWorld = b.worldTransform;

		bool hit = false;
		if (a.sphere && b.sphere) {
			hit = Collision::SphereSphere(a.sphere->GetWorldSphere(aWorld), b.sphere->GetWorldSphere(bWorld));
		} else if (a.obb && b.obb) {
			hit = Collision::OBBOBB(a.obb->GetWorldOBB(aWorld), b.obb->GetWorldOBB(bWorld));
		} else if (a.sphere && b.obb) {
			hit = Collision::OBBSphere(b.obb->GetWorldOBB(bWorld), a.sphere->GetWorldSphere(aWorld));
		} else if (a.obb && b.sphere) {
			hit = Collision::OBBSphere(a.obb->GetWorldOBB(aWorld), b.sphere->GetWorldSphere(bWorld));
		}
		if (!hit) return;

		// 片方でもTriggerならこのペアはTrigger重なり、両方Solidの場合のみSolid重なり
		if (a.base->isTrigger || b.base->isTrigger) {
			a.overlappingTrigger = true; b.overlappingTrigger = true;
			a.triggerOverlapsThisFrame.push_back(b.obj);
			b.triggerOverlapsThisFrame.push_back(a.obj);

			// 前フレームは重なっていなかった相手のときだけOnTriggerEnterを呼ぶ（Unityの
			// 「重なった瞬間に1回だけ呼ばれる」挙動に合わせる。毎フレーム呼ぶと、HPを減らす等の
			// 処理を書いたときに重なっている間ずっと連打されてしまうため）
			bool wasAlreadyOverlapping = std::find(a.base->triggerOverlapsLastFrame_.begin(),
				a.base->triggerOverlapsLastFrame_.end(), b.obj) != a.base->triggerOverlapsLastFrame_.end();
			if (!wasAlreadyOverlapping) {
				a.obj->OnTriggerEnter(*b.obj);
				b.obj->OnTriggerEnter(*a.obj);
			}
		} else {
			a.overlappingSolid = true; b.overlappingSolid = true;

			// Stop中（isPlaying=false）は押し戻しを行わない。重なりの検知・色分け表示だけは
			// 常時行い、Gizmoでの自由な配置を妨げないようにする
			if (!isPlaying) return;

			// 両方Solid：実際に押し戻して重なりを解消する（半分ずつ均等に分配）
			Vector3 normal{ 0.0f, 0.0f, 0.0f }; // a→b向き
			float   depth = 0.0f;
			bool    resolved = false;
			if (a.sphere && b.sphere) {
				resolved = Collision::SphereSpherePenetration(a.sphere->GetWorldSphere(aWorld), b.sphere->GetWorldSphere(bWorld), normal, depth);
			} else if (a.obb && b.obb) {
				resolved = Collision::OBBOBBPenetration(a.obb->GetWorldOBB(aWorld), b.obb->GetWorldOBB(bWorld), normal, depth);
			} else if (a.sphere && b.obb) {
				// OBBSpherePenetrationはobb→sphere向きを返すため、a(sphere)→b(obb)向きに揃えるため反転する
				resolved = Collision::OBBSpherePenetration(b.obb->GetWorldOBB(bWorld), a.sphere->GetWorldSphere(aWorld), normal, depth);
				normal = normal * -1.0f;
			} else if (a.obb && b.sphere) {
				resolved = Collision::OBBSpherePenetration(a.obb->GetWorldOBB(aWorld), b.sphere->GetWorldSphere(bWorld), normal, depth);
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

				// GravityFlipComponent版（GraviTwist用、4方向重力）。上のGravityComponent分岐と
				// 完全に対称な形にする。「押し戻しベクトルと現在の重力方向の内積が負」＝
				// 「重力方向と逆向きに押し戻された」＝「落下方向にある壁に着地した」ことを表す
				// （Y軸決め打ちのaDelta.y>0判定を、Dot(押し戻し, 重力方向)<0という
				// 一般化した式に置き換えたもの）
				if (GravityFlipComponent* aFlip = a.obj->GetComponent<GravityFlipComponent>()) {
					Vector3 fallDir = GravityDirectionToVector(aFlip->direction);
					if (VectorMath::Dot(aDelta, fallDir) < 0.0f) {
						aFlip->velocity = 0.0f;
						if (!aFlip->isGrounded) aFlip->NotifyLanded();
						aFlip->isGrounded = true;
					}
				}
				if (GravityFlipComponent* bFlip = b.obj->GetComponent<GravityFlipComponent>()) {
					Vector3 fallDir = GravityDirectionToVector(bFlip->direction);
					if (VectorMath::Dot(bDelta, fallDir) < 0.0f) {
						bFlip->velocity = 0.0f;
						if (!bFlip->isGrounded) bFlip->NotifyLanded();
						bFlip->isGrounded = true;
					}
				}
			}
		}
	};

	// グリッド近傍同士はi<jの片方向のみ処理する（自分自身・逆順の重複を除く）
	for (size_t i = 0; i < entries.size(); i++) {
		for (size_t j : collectNearby(entries[i])) {
			if (j <= i) continue;
			resolvePair(i, j);
		}
	}

	// oversized（壁等）とグリッド内の全エントリとの組は、順序に関係なく1回だけ判定する。
	// oversized同士はShouldLayersCollideがObstacle同士を弾くため実質判定されない
	for (size_t oi : oversizedEntries) {
		for (size_t i = 0; i < entries.size(); i++) {
			if (i == oi) continue;
			resolvePair((std::min)(oi, i), (std::max)(oi, i));
		}
	}

	// 今フレームのTrigger重なり一覧を各Colliderの「前フレーム」状態として保存する。
	// drawDebugの早期returnより前で、判定を行った全フレームで必ず更新する
	// （ここを飛ばすとGameビュー表示中だけOnTriggerEnterが毎フレーム連打される不具合になる）
	for (auto& e : entries) {
		e.base->triggerOverlapsLastFrame_ = e.triggerOverlapsThisFrame;
	}

	if (!drawDebug) return; // Gameビュー表示中は判定・押し戻しのみ行い、ワイヤーフレーム描画は省略する

	// 描画は各コンポーネント自身のDrawWireframeに委譲する（ColliderSystemは判定結果の色だけ渡す）。
	// 1つのオブジェクトが複数と同時に重なる場合はSolid重なりを優先して赤にする
	// （「ぶつかって止まる」方が「すり抜けて拾う」より目視上の優先度が高いため）
	for (auto& e : entries) {
		Vector4 color = e.overlappingSolid   ? kSolidOverlapColor
		              : e.overlappingTrigger ? kTriggerOverlapColor
		                                     : kNoOverlapColor;
		if (e.sphere) e.sphere->DrawWireframe(renderer, e.worldTransform, color, view, proj);
		if (e.obb)    e.obb->DrawWireframe(renderer, e.worldTransform, color, view, proj);
	}
}
