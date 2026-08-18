#pragma once

// シーン内のGameObjectを検索する際に使うタグ名の集約。SceneBase::Render（MainCamera/Playerタグに
// よるオブジェクト解決）とPlayScene（Player/Enemy/EnemySpawnerタグによるゲームロジック）の両方から
// 参照されるため、無名namespace（各.cpp内で完結する定数）ではなく、ヘッダ側の名前空間定数として
// 1箇所にまとめる。値そのものは保存済みscene.jsonのGameObject::tag文字列と一致している必要があるため、
// 変更する場合は既存シーンのtag値も合わせて書き換えること
namespace GameTags {
	inline constexpr const char* kPlayer       = "Player";
	inline constexpr const char* kMainCamera   = "MainCamera";
	inline constexpr const char* kEnemy        = "Enemy";
	inline constexpr const char* kEnemySpawner = "EnemySpawner";
}
