#include "EnemySpawnManager.h"
#include "PlayScene.h"
#include "GameTags.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Math/VectorMath.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
	// フィールド範囲・敵同士の最小距離・グリッドセル間隔は、EnemySpawner
	// （ReflexEnemySpawnerComponent::spawnRangeMinX/MaxX/MinY/MaxY、minSpawnDistance、
	// spawnGridCellSize）がInspectorから調整できる。以下はEnemySpawnerが見つからない場合のみ使う
	// フォールバック定数（壁が±10付近にある想定で、壁の厚み・敵自身の半径分の余裕を持たせた内側±8）
	constexpr float kSpawnRangeMin = -8.0f;
	constexpr float kSpawnRangeMax = 8.0f;

	// 他の敵・プレイヤーとこれ未満の距離にはスポーンさせない（グリッドが尽きた場合の
	// フォールバック抽選でのみ使う。通常時はグリッド配置自体が重なりを防ぐ）
	constexpr float kMinSpawnDistance = 2.0f;

	// 距離条件を満たす座標が見つからない場合の再抽選回数の上限（無限ループ防止）
	constexpr int kMaxSpawnAttempts = 30;

	// スポーン用グリッドのセル間隔。敵1体の当たり判定サイズ（halfSize=0.5、直径1）より
	// 十分広く取り、隣接セルに配置された敵同士が接触しないようにする
	constexpr float kSpawnGridCellSize = 1.5f;

	// テンプレートを見つけられなかった場合のフォールバック見た目・サイズ
	constexpr float kFallbackHalfSize = 0.5f;

	// ヒエラルキー上でスポーン物をまとめるフォルダ代わりのGameObjectのtag名
	constexpr const char* kEnemyFolderTag = "Enemies";
	constexpr const char* kParticleFolderTag = "Particles";

	// 全方位ランダム方向を一様抽選する際、方位角theta（SpawnParticleBurstAt参照）の範囲として使う一周分
	constexpr float kTwoPi = 6.2831853f;
}

EnemySpawnManager::TemplateShape EnemySpawnManager::DetermineTemplateShape(GameObject& templateObj) {
	if (templateObj.GetComponent<SphereRenderComponent>()) return TemplateShape::kSphere;
	if (templateObj.GetComponent<TriangleRenderComponent>()) return TemplateShape::kTriangle;
	return TemplateShape::kCube;
}

std::vector<Vector3> EnemySpawnManager::BuildShuffledSpawnGrid(PlayScene& scene) {
	// 現在生存している敵・プレイヤーが占有しているマスは候補から除外する。ここを素通りして
	// フィールド全体を毎回無条件に返していたため、常時湧きの1体補充のたびに「既に敵/プレイヤーが
	// いるマス」が候補に混ざり、偶然重なってスポーンすることがあった。
	//
	// 閾値は占有元の種類で使い分ける：敵はグリッド座標そのもので生成されている（過去のスポーンで
	// 必ず格子点ぴったりに置かれている）ため、セル半分の狭い閾値で正確に検出できる。一方
	// プレイヤーは自由に移動する任意の座標（格子点からズレているのが常態）のため、狭い閾値では
	// すぐ近くの格子点しか弾けず、1〜2マス離れた「見た目にはまだ近い」マスに湧いてしまっていた。
	// プレイヤーの除外半径は「安全に離れていてほしい距離」であるplayerExclusionRadiusをそのまま使う。
	float spawnRangeMinX = kSpawnRangeMin;
	float spawnRangeMaxX = kSpawnRangeMax;
	float spawnRangeMinY = kSpawnRangeMin;
	float spawnRangeMaxY = kSpawnRangeMax;
	float gridCellSize = kSpawnGridCellSize;
	float exclusionRadius = kMinSpawnDistance;
	if (GameObject* spawner = scene.FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			spawnRangeMinX = config->spawnRangeMinX;
			spawnRangeMaxX = config->spawnRangeMaxX;
			spawnRangeMinY = config->spawnRangeMinY;
			spawnRangeMaxY = config->spawnRangeMaxY;
			gridCellSize = config->spawnGridCellSize;
			exclusionRadius = config->playerExclusionRadius;
		}
	}

	struct OccupiedPoint { Vector3 pos; float radius; };
	std::vector<OccupiedPoint> occupied;
	for (auto& obj : scene.objects_) {
		if (obj->tag != GameTags::kEnemy) continue;
		// SpawnMoveComponentが付いていて演出中（finished==false）の敵は、現在位置がまだ
		// startPos→targetPosの移動途中にあり、本来の着地マスにいない。ここで現在位置だけを
		// 占有マスとして報告すると、その敵の「本来の着地マス」がまだ空いていると誤判定され、
		// 演出完了後に別の敵と同じマスへ重複してスポーンしてしまっていた。演出中はtargetPos
		// （最終的にそのマスへ収まる位置）を占有マスとして扱うことでこれを防ぐ
		if (auto* spawnMove = obj->GetComponent<SpawnMoveComponent>()) {
			if (!spawnMove->finished) {
				occupied.push_back({ spawnMove->targetPos, gridCellSize * 0.5f });
				continue;
			}
		}
		occupied.push_back({ obj->GetTransform().translation, gridCellSize * 0.5f });
	}
	if (GameObject* player = scene.FindObjectByTag(GameTags::kPlayer)) {
		occupied.push_back({ player->GetTransform().translation, exclusionRadius });
	}
	auto isOccupied = [&](const Vector3& cell) {
		for (const OccupiedPoint& occ : occupied) {
			if (VectorMath::Length(cell - occ.pos) < occ.radius) return true;
		}
		return false;
		};

	std::vector<Vector3> cells;
	for (float x = spawnRangeMinX; x <= spawnRangeMaxX; x += gridCellSize) {
		for (float y = spawnRangeMinY; y <= spawnRangeMaxY; y += gridCellSize) {
			Vector3 cell{ x, y, 0.0f };
			if (!isOccupied(cell)) cells.push_back(cell);
		}
	}
	static std::mt19937 rng{ std::random_device{}() };
	std::shuffle(cells.begin(), cells.end(), rng);
	return cells;
}

Vector3 EnemySpawnManager::PickEnemySpawnPosition(PlayScene& scene, std::vector<Vector3>& cells) {
	static std::mt19937 rng{ std::random_device{}() };

	// グリッドに空きがある間は、シャッフル済みの末尾から1つ取り出して消費する
	if (!cells.empty()) {
		Vector3 candidate = cells.back();
		cells.pop_back();
		return candidate;
	}

	// グリッドを使い切った場合（要求数がフィールド容量を超えた場合）は、従来通り
	// minSpawnDistance以上離れた地点をランダム再抽選するフォールバックに切り替える
	float spawnRangeMinX = kSpawnRangeMin;
	float spawnRangeMaxX = kSpawnRangeMax;
	float spawnRangeMinY = kSpawnRangeMin;
	float spawnRangeMaxY = kSpawnRangeMax;
	float minSpawnDistance = kMinSpawnDistance;
	float playerExclusionRadius = kMinSpawnDistance;
	if (GameObject* spawner = scene.FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			spawnRangeMinX = config->spawnRangeMinX;
			spawnRangeMaxX = config->spawnRangeMaxX;
			spawnRangeMinY = config->spawnRangeMinY;
			spawnRangeMaxY = config->spawnRangeMaxY;
			minSpawnDistance = config->minSpawnDistance;
			playerExclusionRadius = config->playerExclusionRadius;
		}
	}

	std::uniform_real_distribution<float> distX(spawnRangeMinX, spawnRangeMaxX);
	std::uniform_real_distribution<float> distY(spawnRangeMinY, spawnRangeMaxY);
	Vector3 candidate{ 0.0f, 0.0f, 0.0f };
	for (int attempt = 0; attempt < kMaxSpawnAttempts; attempt++) {
		candidate = { distX(rng), distY(rng), 0.0f };

		bool tooClose = false;
		if (GameObject* player = scene.FindObjectByTag(GameTags::kPlayer)) {
			if (VectorMath::Length(candidate - player->GetTransform().translation) < playerExclusionRadius) tooClose = true;
		}
		if (!tooClose) {
			for (auto& obj : scene.objects_) {
				if (obj->tag != GameTags::kEnemy) continue;
				if (VectorMath::Length(candidate - obj->GetTransform().translation) < minSpawnDistance) {
					tooClose = true;
					break;
				}
			}
		}
		if (!tooClose) return candidate;
	}
	return candidate; // 上限回数まで条件を満たせなかった場合は最後の候補をそのまま使う
}

void EnemySpawnManager::SpawnEnemyAt(PlayScene& scene, const Vector3& position, const std::string& templateTag) {
	EnemyTemplateData data = ReadEnemyTemplateData(scene, templateTag);

	// EnemySpawnerのsizeScaleMin/sizeScaleMaxの範囲でランダムな倍率を抽選する。
	// EnemySpawnerが見つからない場合は等倍（1.0）のまま
	if (GameObject* spawner = scene.FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			float scaleMin = config->sizeScaleMin;
			float scaleMax = (std::max)(config->sizeScaleMax, scaleMin);
			static std::mt19937 rng{ std::random_device{}() };
			std::uniform_real_distribution<float> dist(scaleMin, scaleMax);
			data.sizeScale = dist(rng);
		}
	}

	BuildEnemyFromTemplateData(scene, position, templateTag, data);
}

EnemySpawnManager::EnemyTemplateData EnemySpawnManager::ReadEnemyTemplateData(PlayScene& scene, const std::string& templateTag) {
	// templateTagを持つテンプレートGameObjectから見た目（形状＋色）・当たり判定（形状＋サイズ）・
	// ReflexEnemyComponentのパラメータを複製する。見つからない場合は赤い立方体
	// （既定サイズ・既定パラメータ）にフォールバックする
	EnemyTemplateData data;
	data.size = kFallbackHalfSize;

	GameObject* templateObj = scene.FindObjectByTag(templateTag);
	if (!templateObj) return data;

	// 見た目：具体型を判定して形状を決め、共通基底（color等）から色を取る
	if (auto* templateRender = templateObj->GetComponent<RenderComponentBase>()) {
		data.color = templateRender->color;
		data.shape = DetermineTemplateShape(*templateObj);
	}
	// テクスチャ：TextureSelectorComponentはtextureHandleを実行時ハンドルとしてしか
	// 持たない（保存対象外）ため、ToJsonが書き出す「登録済みテクスチャ名」経由で複製する
	if (auto* textureSelector = templateObj->GetComponent<TextureSelectorComponent>()) {
		nlohmann::json textureJson;
		textureSelector->ToJson(textureJson);
		data.textureName = textureJson.value("textureName", std::string());
	}
	// 当たり判定：OBB/Sphereどちらが付いているかを判定し、サイズを1つのfloatに正規化する
	if (auto* obbCollider = templateObj->GetComponent<OBBColliderComponent>()) {
		data.colliderShape = TemplateColliderShape::kObb;
		data.size = obbCollider->halfSize.x;
	} else if (auto* sphereCollider = templateObj->GetComponent<SphereColliderComponent>()) {
		data.colliderShape = TemplateColliderShape::kSphere;
		data.size = sphereCollider->radius;
	} else {
		data.colliderShape = TemplateColliderShape::kNone;
	}
	if (auto* templateEnemy = templateObj->GetComponent<ReflexEnemyComponent>()) {
		data.hitShakeStrength = templateEnemy->hitShakeStrength;
		data.hitShakeDuration = templateEnemy->hitShakeDuration;
		data.hitStopDuration = templateEnemy->hitStopDuration;
		data.maxHp = templateEnemy->maxHp;
	}
	if (auto* templateHealthBar = templateObj->GetComponent<ReflexEnemyHealthBarComponent>()) {
		data.hasHealthBar = true;
		data.healthBarWidth = templateHealthBar->width;
		data.healthBarHeight = templateHealthBar->height;
		data.healthBarHeightOffset = templateHealthBar->heightOffset;
		data.healthBarBackgroundColor = templateHealthBar->backgroundColor;
		data.healthBarFillColor = templateHealthBar->fillColor;
	}
	if (auto* templateRotator = templateObj->GetComponent<RotatorComponent>()) {
		data.hasRotator = true;
		data.rotatorRandomizeOnSpawn = templateRotator->randomizeOnSpawn;
		data.rotatorSpeedX = templateRotator->speedX;
		data.rotatorSpeedY = templateRotator->speedY;
		data.rotatorSpeedZ = templateRotator->speedZ;
		data.rotatorRandomSpeedMin = templateRotator->randomSpeedMin;
		data.rotatorRandomSpeedMax = templateRotator->randomSpeedMax;
	}
	if (auto* templateHitSound = templateObj->GetComponent<HitSoundComponent>()) {
		data.hasHitSound = true;
		nlohmann::json hitSoundJson;
		templateHitSound->ToJson(hitSoundJson);
		data.hitSoundAudioName = hitSoundJson.value("audioName", std::string());
		data.hitSoundVolume = hitSoundJson.value("volume", 1.0f);
	}
	if (auto* templateSpawnSound = templateObj->GetComponent<SpawnSoundComponent>()) {
		data.hasSpawnSound = true;
		nlohmann::json spawnSoundJson;
		templateSpawnSound->ToJson(spawnSoundJson);
		data.spawnSoundAudioName = spawnSoundJson.value("audioName", std::string());
		data.spawnSoundVolume = spawnSoundJson.value("volume", 1.0f);
	}
	if (auto* templateSpawnMove = templateObj->GetComponent<SpawnMoveComponent>()) {
		data.hasSpawnMove = true;
		data.spawnMoveZOffset = templateSpawnMove->zOffset;
		data.spawnMoveDuration = templateSpawnMove->duration;
		data.spawnMoveEasing = templateSpawnMove->easing;
	}

	return data;
}

void EnemySpawnManager::BuildEnemyFromTemplateData(PlayScene& scene, const Vector3& position,
	const std::string& templateTag, const EnemyTemplateData& data) {
	GameObject& enemy = scene.CreateObject("Enemy");
	enemy.tag = GameTags::kEnemy;
	enemy.GetTransform().translation = position;
	// スポーン時のサイズランダム化を見た目・当たり判定の両方に連動して適用する
	// （data.size側では掛けない＝halfSize×scaleの二重適用を防ぐ）
	enemy.GetTransform().scale = { data.sizeScale, data.sizeScale, data.sizeScale };
	// ヒエラルキーが敵だらけでフラットに埋まらないよう、"Enemies"フォルダの子としてぶら下げる
	enemy.SetParent(&scene.GetOrCreateGroupFolder(kEnemyFolderTag));

	if (data.hasSpawnMove) {
		// 本来のスポーン地点(position)をtargetPos、そこからZ方向にzOffset離れた地点をstartPosにする
		auto* spawnMove = enemy.AddComponent<SpawnMoveComponent>();
		spawnMove->targetPos = position;
		spawnMove->startPos = position + Vector3{ 0.0f, 0.0f, data.spawnMoveZOffset };
		spawnMove->duration = data.spawnMoveDuration;
		spawnMove->easing = data.spawnMoveEasing;
		spawnMove->elapsed = 0.0f;
		spawnMove->finished = false;
		enemy.GetTransform().translation = spawnMove->startPos;
	}

	RenderComponentBase* render = nullptr;
	switch (data.shape) {
		case TemplateShape::kSphere:   render = enemy.AddComponent<SphereRenderComponent>(); break;
		case TemplateShape::kTriangle: render = enemy.AddComponent<TriangleRenderComponent>(); break;
		default:                       render = enemy.AddComponent<CubeRenderComponent>(); break;
	}
	render->color = data.color;

	// テンプレートにテクスチャが設定されていれば、同じテクスチャ名でTextureSelectorComponentを
	// 作り直す（コンストラクタ引数必須のためComponentRegistry::Create経由）
	if (!data.textureName.empty()) {
		ComponentLoadContext ctx = scene.MakeComponentLoadContext();
		nlohmann::json textureData;
		textureData["textureName"] = data.textureName;
		ComponentRegistry::Create("TextureSelector", enemy, ctx, textureData);
	}

	auto* enemyComponent = enemy.AddComponent<ReflexEnemyComponent>();
	enemyComponent->hitShakeStrength = data.hitShakeStrength;
	enemyComponent->hitShakeDuration = data.hitShakeDuration;
	enemyComponent->hitStopDuration = data.hitStopDuration;
	enemyComponent->maxHp = data.maxHp;
	enemyComponent->hp = data.maxHp; // 複製時は必ず満タンのHPでスポーンする
	enemyComponent->spawnedFromTag = templateTag; // 撃破時、同じ種類を1体補充するために覚えておく

	if (data.hasHealthBar) {
		auto* healthBar = enemy.AddComponent<ReflexEnemyHealthBarComponent>(enemyComponent);
		healthBar->width = data.healthBarWidth;
		healthBar->height = data.healthBarHeight;
		healthBar->heightOffset = data.healthBarHeightOffset;
		healthBar->backgroundColor = data.healthBarBackgroundColor;
		healthBar->fillColor = data.healthBarFillColor;
	}

	if (data.hasRotator) {
		auto* rotator = enemy.AddComponent<RotatorComponent>();
		if (data.rotatorRandomizeOnSpawn) {
			rotator->randomizeOnSpawn = true;
			rotator->randomSpeedMin = data.rotatorRandomSpeedMin;
			rotator->randomSpeedMax = data.rotatorRandomSpeedMax;
			rotator->Randomize();
		} else {
			rotator->speedX = data.rotatorSpeedX;
			rotator->speedY = data.rotatorSpeedY;
			rotator->speedZ = data.rotatorSpeedZ;
		}
	}

	if (data.hasHitSound && !data.hitSoundAudioName.empty()) {
		int audioIndex = -1;
		for (size_t i = 0; i < scene.projectAudioClips_.size(); i++) {
			if (scene.projectAudioClips_[i].displayName == data.hitSoundAudioName) { audioIndex = static_cast<int>(i); break; }
		}
		enemy.AddComponent<HitSoundComponent>(&scene.projectAudioClips_, audioIndex, data.hitSoundVolume);
	}

	if (data.hasSpawnSound && !data.spawnSoundAudioName.empty()) {
		int audioIndex = -1;
		for (size_t i = 0; i < scene.projectAudioClips_.size(); i++) {
			if (scene.projectAudioClips_[i].displayName == data.spawnSoundAudioName) { audioIndex = static_cast<int>(i); break; }
		}
		auto* spawnSound = enemy.AddComponent<SpawnSoundComponent>(&scene.projectAudioClips_, audioIndex, data.spawnSoundVolume);
		spawnSound->Play(); // スポーンした瞬間に1回だけ鳴らす
	}

	switch (data.colliderShape) {
		case TemplateColliderShape::kSphere: {
			auto* collider = enemy.AddComponent<SphereColliderComponent>();
			collider->isTrigger = true;
			collider->radius = data.size;
			break;
		}
		case TemplateColliderShape::kNone:
			break;
		default: {
			auto* collider = enemy.AddComponent<OBBColliderComponent>();
			collider->isTrigger = true;
			collider->halfSize = { data.size, data.size, data.size };
			break;
		}
	}

	scene.RebuildDerivedLists(); // gizmoTargets_に新規オブジェクトを反映する
}

void EnemySpawnManager::SpawnParticleBurstAt(PlayScene& scene, const Vector3& position, const std::string& templateTag) {
	GameObject* templateObj = scene.FindObjectByTag(templateTag);
	if (!templateObj) return; // テンプレートが無ければ演出なしで諦める（EnemySpawnerと違い必須要素ではない）

	auto* emitterConfig = templateObj->GetComponent<ParticleEmitterComponent>();
	if (!emitterConfig) return;

	// 見た目：テンプレートの具体型を判定して形状を決め、共通基底（color等）から色を取る
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	TemplateShape shape = TemplateShape::kCube;
	if (auto* templateRender = templateObj->GetComponent<RenderComponentBase>()) {
		color = templateRender->color;
		shape = DetermineTemplateShape(*templateObj);
	}

	static std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> zDist(-1.0f, 1.0f);
	std::uniform_real_distribution<float> thetaDist(0.0f, kTwoPi);
	std::uniform_real_distribution<float> sizeStartDist(emitterConfig->sizeStartMin, emitterConfig->sizeStartMax);
	std::uniform_real_distribution<float> sizeEndDist(emitterConfig->sizeEndMin, emitterConfig->sizeEndMax);
	std::uniform_real_distribution<float> speedDist(emitterConfig->speedMin, emitterConfig->speedMax);

	// "Particles"フォルダの子としてぶら下げる（フォルダはTransformが原点固定のため、
	// 子のtranslationはそのままワールド座標として扱われる）
	GameObject& folder = scene.GetOrCreateGroupFolder(kParticleFolderTag);
	// フォルダ自体も中身（パーティクル）と同じく実行時にしか意味を持たない一時的なコンテナのため
	// 保存対象外にする
	folder.excludeFromSave = true;

	for (int i = 0; i < emitterConfig->count; i++) {
		// 球面上の一様ランダム方向（z軸を一様抽選し、その高さの円周上をthetaで一様抽選する
		// 標準的な手法。緯度経度を直接一様抽選すると極付近に偏るため使わない）
		float z = zDist(rng);
		float theta = thetaDist(rng);
		float r = std::sqrt((std::max)(0.0f, 1.0f - z * z));
		Vector3 direction = { r * std::cos(theta), r * std::sin(theta), z };

		GameObject& particle = scene.CreateObject("Particle");
		particle.excludeFromPicking = true;
		particle.excludeFromSave = true;
		particle.GetTransform().translation = position;
		particle.SetParent(&folder);

		RenderComponentBase* render = nullptr;
		switch (shape) {
			case TemplateShape::kSphere:   render = particle.AddComponent<SphereRenderComponent>(); break;
			case TemplateShape::kTriangle: render = particle.AddComponent<TriangleRenderComponent>(); break;
			default:                       render = particle.AddComponent<CubeRenderComponent>(); break;
		}
		render->color = color;

		auto* particleComponent = particle.AddComponent<ParticleComponent>();
		particleComponent->direction = direction;
		particleComponent->speed = speedDist(rng);
		particleComponent->sizeStart = sizeStartDist(rng);
		particleComponent->sizeEnd = sizeEndDist(rng);
		particleComponent->sizeEasing = emitterConfig->sizeEasing;
		particleComponent->lifeTime = emitterConfig->lifeTime;
		particle.GetTransform().scale = { particleComponent->sizeStart, particleComponent->sizeStart, particleComponent->sizeStart };

		if (emitterConfig->enableRotation) {
			auto* rotator = particle.AddComponent<RotatorComponent>();
			rotator->randomizeOnSpawn = true;
			rotator->randomSpeedMin = emitterConfig->rotationSpeedMin;
			rotator->randomSpeedMax = emitterConfig->rotationSpeedMax;
			rotator->Randomize();
		}
	}

	scene.RebuildDerivedLists(); // gizmoTargets_に新規オブジェクトを反映する
}
