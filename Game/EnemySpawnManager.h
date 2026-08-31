#pragma once
#include "../Math/Easing.h"
#include "../Math/MathTypes.h"
#include <string>
#include <vector>

class PlayScene;
class GameObject;

// PlayScene::SpawnEnemyAt/SpawnParticleBurstAt及びその周辺（テンプレート読み取り・グリッド
// 配置計算）を切り出した敵スポーン専用クラス。PlaySceneは「いつ・何を・何体スポーンするか」
// （フェーズ制御・タイミング・respawnQueue_の管理）に専念し、こちらは「実際にテンプレートから
// GameObjectを組み立てる」処理に専念する。
//
// CreateObject/FindObjectByTag/RebuildDerivedLists等はSceneBaseのprotectedメンバのため、
// このクラスはPlaySceneのfriendとして宣言してもらい、PlayScene&を経由してアクセスする
// （ReflexPathVisualizerのような「引数だけで完結する純粋ヘルパー」とは異なり、シーンの内部
// 状態に深く食い込む処理のため、friendによるアクセス許可が実態に即している）
class EnemySpawnManager {
public:
	// テンプレートの描画形状（Cube/Sphere/Triangle）を表す
	enum class TemplateShape { kCube, kSphere, kTriangle };

	// テンプレートのコライダー形状（OBB/Sphere）。サイズはhalfSize.x/radiusという別名の
	// スカラー値だが、どちらも「中心からの片側の長さ」という同じ意味として1つのfloatに統一する
	enum class TemplateColliderShape { kNone, kObb, kSphere };

	// テンプレートGameObjectから読み取ったスポーンパラメータの集約。SpawnEnemyAtが
	// 「読み取り」と「組み立て」の2段階に分かれるようにするための橋渡し役
	struct EnemyTemplateData {
		Vector4 color = { 0.9f, 0.2f, 0.2f, 1.0f };
		TemplateShape shape = TemplateShape::kCube;
		TemplateColliderShape colliderShape = TemplateColliderShape::kObb;
		float size = 0.5f; // テンプレートが見つからない場合のフォールバック半径

		// スポーン時のサイズランダム化（EnemySpawner::sizeScaleMin/Max）が使う、見た目・当たり判定
		// 両方に掛ける倍率。data.size側では掛けない（halfSize×scaleの二重適用を防ぐため）
		float sizeScale = 1.0f;
		float hitShakeStrength = 0.25f;
		float hitShakeDuration = 0.15f;
		float hitStopDuration = 0.05f;
		float maxHp = 10.0f;
		std::string textureName;

		bool hasHealthBar = false;
		float healthBarWidth = 1.2f;
		float healthBarHeight = 0.15f;
		float healthBarHeightOffset = 1.0f;
		Vector4 healthBarBackgroundColor = { 0.15f, 0.15f, 0.15f, 0.8f };
		Vector4 healthBarFillColor = { 0.2f, 0.9f, 0.2f, 1.0f };

		bool hasRotator = false;
		bool rotatorRandomizeOnSpawn = false;
		float rotatorSpeedX = 0.0f;
		float rotatorSpeedY = 90.0f;
		float rotatorSpeedZ = 0.0f;
		float rotatorRandomSpeedMin = 30.0f;
		float rotatorRandomSpeedMax = 180.0f;

		bool hasHitSound = false;
		std::string hitSoundAudioName;
		float hitSoundVolume = 1.0f;

		bool hasSpawnSound = false;
		std::string spawnSoundAudioName;
		float spawnSoundVolume = 1.0f;

		bool hasSpawnMove = false;
		float spawnMoveZOffset = 10.0f;
		float spawnMoveDuration = 0.5f;
		Easing::Type spawnMoveEasing = Easing::Type::kOutCubic;
	};

	// 1体分のEnemy GameObjectを、templateTagを持つテンプレートGameObjectの見た目・当たり判定
	// サイズ・ReflexEnemyComponentのパラメータを複製して生成する。EnemySpawner::sizeScaleMin/Max
	// の範囲でランダムな倍率も抽選する
	void SpawnEnemyAt(PlayScene& scene, const Vector3& position, const std::string& templateTag);

	// templateTagを持つParticleTemplateComponent付きGameObjectの見た目・ParticleEmitterComponent
	// 設定を読み取り、position位置から全方位ランダムな方向へ指定個数分のParticleComponent付き
	// GameObjectを一括生成する（敵撃破時等、「四方八方に飛び散る」演出向け）
	void SpawnParticleBurstAt(PlayScene& scene, const Vector3& position, const std::string& templateTag);

	// フィールド内をkGridCellSize間隔で敷き詰めたグリッド点のうち、現在生存している敵・
	// プレイヤーが占有しているマスを除いたものだけをランダムな順序で返す。
	// PickEnemySpawnPositionが順に消費していくことで、要求数がグリッドの容量以内である限り
	// 敵同士が絶対に重ならない配置になる
	std::vector<Vector3> BuildShuffledSpawnGrid(PlayScene& scene);

	// フィールド内(壁の内側)をkGridCellSize間隔のグリッドに区切り、シャッフル済みの未使用セル
	// 一覧cellsから1つ取り出して消費する（呼び出しごとに違うセルを返す＝敵同士が絶対に重ならない）。
	// cellsが尽きた場合はkMinSpawnDistance以上離れた地点をランダム再抽選するフォールバックに
	// 切り替える（フィールド容量を超える数を要求された場合の保険）
	Vector3 PickEnemySpawnPosition(PlayScene& scene, std::vector<Vector3>& cells);

private:
	// テンプレートの具体型からTemplateShapeを判定する。SpawnEnemyAt/SpawnParticleBurstAtの
	// 両方が同じ判定を必要とするため共通化した
	static TemplateShape DetermineTemplateShape(GameObject& templateObj);

	// templateTagを持つテンプレートGameObjectから見た目・当たり判定・ReflexEnemyComponent
	// パラメータ等を読み取る（見つからない場合はEnemyTemplateDataの既定値＝フォールバック値のまま返す）
	EnemyTemplateData ReadEnemyTemplateData(PlayScene& scene, const std::string& templateTag);

	// ReadEnemyTemplateDataの結果から実際にEnemy GameObjectを組み立てる
	void BuildEnemyFromTemplateData(PlayScene& scene, const Vector3& position,
		const std::string& templateTag, const EnemyTemplateData& data);
};
