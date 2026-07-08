#pragma once
#include <cstddef>

// 当たり判定のレイヤー分け。「所属レイヤー」の種類だけをここで定義する。
// 「どのレイヤーと衝突するか」はColliderComponentBase::collidesWithが
// オブジェクトごとに個別に持つため、ここには持たない
enum class CollisionLayer {
	kDefault,     // 未分類。とりあえず全レイヤーと判定させたいときのデフォルト
	kPlayer,      // プレイヤー本体
	kObstacle,    // 障害物。ぶつかると止まる/ゲームオーバー等を想定（Solid運用）
	kItem,        // アイテム。プレイヤーがすり抜けて拾う想定（Trigger運用）
	kEnvironment, // 床/壁など背景の当たり判定

	kCount, // 要素数。配列サイズ/ループ境界にのみ使う番兵で、実際のレイヤー値としては使わない
};
