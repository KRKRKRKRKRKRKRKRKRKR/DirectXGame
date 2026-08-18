#include "PlayScene.h"
#include "GameTags.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Math/VectorMath.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
	// フィールド範囲（TopWall/BottomWall/LeftWall/RightWallが±10付近にあるため、
	// 壁の厚み・敵自身の半径分の余裕を持たせて内側±8までを配置可能範囲とする）
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

	// 準備フェーズ中、1体スポーンしてから次の1体をスポーンするまでの間隔（秒）の既定値。
	// 通常はEnemySpawnerのReflexEnemySpawnerComponent::respawnInterval（Inspectorで調整可能）を
	// 使うため、これはEnemySpawnerが見つからない場合のフォールバックとしてのみ使われる
	constexpr float kRespawnInterval = 0.3f;

	// テンプレートを見つけられなかった場合のフォールバック見た目・サイズ
	constexpr float kFallbackHalfSize = 0.5f;

	// テンプレートGameObjectをPlay中に隠す際の退避先Y座標（フィールド±8の外側で確実に見えない位置）
	constexpr float kTemplateHideY = -1000.0f;

	// ヒエラルキー上でスポーン物をまとめるフォルダ代わりのGameObjectのtag名
	constexpr const char* kEnemyFolderTag = "Enemies";
	constexpr const char* kParticleFolderTag = "Particles";

	// 全方位ランダム方向を一様抽選する際、方位角theta（SpawnParticleBurstAt参照）の範囲として使う一周分
	constexpr float kTwoPi = 6.2831853f;
}

void PlayScene::DrawSceneSpecificInspectorExtensions(GameObject& selected) {
	// ReflexEnemySpawnerComponent専用UI：spawnEntries[i].tagはシーン内のテンプレート
	// （ReflexEnemyComponent::isTemplate=trueのGameObject）のタグから選ぶコンボにする。
	// 自由入力にするとタグの手打ちミス（例："A"のつもりで"AEnemy"と書く）でテンプレートが
	// 見つからず、PlayScene::SpawnEnemyAtが既定値にフォールバックしてしまう事故が起きるため
	auto* spawnerConfig = selected.GetComponent<ReflexEnemySpawnerComponent>();
	if (!spawnerConfig) return;

	std::vector<std::string> templateTags;
	for (auto& obj : objects_) {
		auto* enemy = obj->GetComponent<ReflexEnemyComponent>();
		if (enemy && enemy->isTemplate && !obj->tag.empty()) {
			templateTags.push_back(obj->tag);
		}
	}

	ImGui::Text("敵の種類（テンプレートのタグ＋出現数）");
	if (templateTags.empty()) {
		ImGui::TextDisabled("  (敵テンプレート（isTemplate=true）を持つGameObjectがありません)");
	}
	for (size_t i = 0; i < spawnerConfig->spawnEntries.size(); i++) {
		ImGui::PushID(static_cast<int>(i));
		auto& entry = spawnerConfig->spawnEntries[i];

		if (ImGui::BeginCombo("テンプレートのタグ", entry.tag.c_str())) {
			for (const auto& t : templateTags) {
				bool isSelected = (entry.tag == t);
				if (ImGui::Selectable(t.c_str(), isSelected)) entry.tag = t;
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::DragInt("出現数", &entry.count, 0.2f, 0, 20);

		bool canRemove = spawnerConfig->spawnEntries.size() > 1;
		ImGui::BeginDisabled(!canRemove);
		if (ImGui::Button("この種類を削除")) {
			spawnerConfig->spawnEntries.erase(spawnerConfig->spawnEntries.begin() + i);
		}
		ImGui::EndDisabled();
		ImGui::Separator();
		ImGui::PopID();
	}
	if (ImGui::Button("種類を追加")) {
		spawnerConfig->spawnEntries.push_back(ReflexEnemySpawnerComponent::SpawnEntry{
			templateTags.empty() ? "" : templateTags.front(), 4 });
	}

	ImGui::Separator();
	ImGui::DragFloat("準備フェーズの補充間隔（秒）", &spawnerConfig->respawnInterval, 0.01f, 0.0f, 5.0f);
}

void PlayScene::OnInitialize() {
	// Camera座標を毎フレーム表示するHUD。表示内容はTextProviderとして1回登録するだけで、
	// 以降はSceneBase::Render内の汎用ループが毎フレーム自動的にUpdateDynamicText()を呼んでくれる
	CreateHud("Camera Coord");

	// 敵の種類は「シーン上に配置されたテンプレートGameObject」（ReflexEnemyComponent::isTemplate=true、
	// 例：tag="A"）で表す。テンプレート自体が本物の敵として動いてしまわないよう、当たり判定を外し・
	// ReflexEnemyComponentを無効化し・フィールド外へ退避させておく
	// （見た目の描画コンポーネントは複製元として残す）。EnemySpawnerのタグ一覧経由ではなく
	// シーン全体を走査するため、EnemySpawnerに登録し忘れたテンプレートも確実に隠せる
	for (auto& obj : objects_) {
		auto* enemy = obj->GetComponent<ReflexEnemyComponent>();
		if (!enemy || !enemy->isTemplate) continue;
		obj->RemoveComponent<OBBColliderComponent>();
		obj->RemoveComponent<SphereColliderComponent>();
		enemy->enabled = false;
		obj->GetTransform().translation.y = kTemplateHideY;
	}

	// パーティクルテンプレート（ParticleTemplateComponent付き）も敵テンプレートと同じ理由で
	// フィールド外へ退避させる。当たり判定を持たないため外す処理は不要
	for (auto& obj : objects_) {
		if (!obj->GetComponent<ParticleTemplateComponent>()) continue;
		obj->GetTransform().translation.y = kTemplateHideY;
	}

	// 注意：OnInitialize()はSceneBase::Initialize()内でLoadScene()より前に呼ばれるため、
	// この時点ではまだResources/Play/scene.jsonの内容（EnemySpawner・テンプレート等）が
	// 読み込まれていない。ここでRespawnEnemiesIfCleared()を呼んでもFindObjectByTagが
	// 何も見つけられず常に空振りする（初回スポーンが出ない原因だった）。
	// 実際の初回スポーンはHandleSceneTransitionInput側の初回フレーム判定（needsInitialSpawn_）で行う
}

ReflexPlayerComponent* PlayScene::GetReflexPlayer() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	if (!player) return nullptr;
	return player->GetComponent<ReflexPlayerComponent>();
}

void PlayScene::HandleSceneTransitionInput() {
	// 初回フレームのみ：OnInitialize()の時点ではまだLoadScene()が済んでおらず
	// EnemySpawner/テンプレートが存在しないため、ここ（LoadScene()完了後に必ず呼ばれる
	// 最初のフレーム）で初回スポーンを行う
	if (needsInitialSpawn_) {
		needsInitialSpawn_ = false;
		RespawnEnemiesIfCleared();
	}

	// ColliderSystem::ResolveAndDrawの直後（Renderの最後）に呼ばれるこのタイミングで、
	// 今フレーム体当たりされた敵をまとめて回収する
	ProcessPendingDestroys();

	// タグ"Player"のReflexPlayerComponentが実行フェーズを完了して準備フェーズに入った瞬間だけ、
	// 直前の実行フェーズ中に倒した敵の補充スポーンを開始する（毎フレーム判定ではなく、
	// 実行フェーズが完全に終わったタイミングに揃える）
	if (auto* reflexPlayer = GetReflexPlayer()) {
		if (reflexPlayer->ConsumeExecutionFinished()) {
			BeginPreparingPhase();
		}
		if (reflexPlayer->GetPhase() == ReflexPlayerComponent::Phase::kPreparing) {
			UpdatePreparingPhase(lastDeltaTime_);
		}
	}

	// デバッグ用キー割り当て（ESCでTitle、F1でGameOverへ遷移）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = "Title";
	if (Input::IsTriggered(DIK_F1))     nextScene_ = "GameOver";

	// タグ"Player"のオブジェクトを毎フレーム見に行き、HealthComponentのHPが0になっていたら
	// GameOverへ遷移する（ポーリング方式。AutoRun/CameraFollowの対象探しと同じやり方に揃えている）
	if (GameObject* player = FindObjectByTag(GameTags::kPlayer)) {
		if (auto* hp = player->GetComponent<HealthComponent>()) {
			if (hp->IsDead()) nextScene_ = "GameOver";
		}
	}
}

GameObject& PlayScene::GetOrCreateGroupFolder(const std::string& tag) {
	if (GameObject* existing = FindObjectByTag(tag)) return *existing;

	// フォルダ自体は見た目・当たり判定を持たない空のGameObject。名前とtagは同じ文字列にする
	// （tagで検索して使い回すのはこの関数自身、名前はヒエラルキー上の表示用）。
	// excludeFromGizmoList=trueにするとヒエラルキーのルート一覧からも消えてしまう
	// （SceneBase::isRootVisible参照）ため、ここでは付けない。3D空間クリックで誤選択しない
	// ようexcludeFromPickingのみ立てる
	GameObject& folder = CreateObject(tag);
	folder.tag = tag;
	folder.excludeFromPicking = true;
	return folder;
}

void PlayScene::ProcessPendingDestroys() {
	std::vector<GameObject*> toDestroy;
	// 撃破された敵・被弾した敵（生死問わず）の座標。ループ完了後にまとめてパーティクルを出す
	// （objects_へのCreateObjectはイテレート中には行えないため）
	std::vector<Vector3> deathParticlePositions;
	std::vector<Vector3> hitParticlePositions;
	for (auto& obj : objects_) {
		if (auto* enemy = obj->GetComponent<EnemyComponent>()) {
			if (enemy->pendingDestroy) toDestroy.push_back(obj.get());
		}
		if (auto* reflexEnemy = obj->GetComponent<ReflexEnemyComponent>()) {
			if (reflexEnemy->pendingHitParticle) {
				hitParticlePositions.push_back(obj->GetWorldTransform().translation);
				reflexEnemy->pendingHitParticle = false;
			}
			// HitSoundComponentはCreateObjectを伴わないため、objects_をイテレート中のこの場で
			// 直接鳴らして問題ない（同じGameObjectに付いていなければ何もしない）
			if (reflexEnemy->pendingHitSound) {
				if (auto* hitSound = obj->GetComponent<HitSoundComponent>()) hitSound->Play();
				reflexEnemy->pendingHitSound = false;
			}
			if (reflexEnemy->pendingDestroy) {
				toDestroy.push_back(obj.get());
				deathParticlePositions.push_back(obj->GetWorldTransform().translation);
				// 準備フェーズ方式：ここでは即座に補充せず、種類だけ覚えておく。実際のスポーンは
				// 実行フェーズ完了後の準備フェーズ（BeginPreparingPhase/UpdatePreparingPhase）で
				// 1体ずつ間隔を空けて行う
				if (!reflexEnemy->spawnedFromTag.empty()) {
					pendingRespawnTags_.push_back(reflexEnemy->spawnedFromTag);
				}
			}
		}
		if (auto* particle = obj->GetComponent<ParticleComponent>()) {
			if (particle->pendingDestroy) toDestroy.push_back(obj.get());
		}
	}
	if (!toDestroy.empty()) DeleteObjects(toDestroy);

	for (const Vector3& position : hitParticlePositions) {
		SpawnParticleBurstAt(position, "EnemyHitParticle");
	}
	for (const Vector3& position : deathParticlePositions) {
		SpawnParticleBurstAt(position, "EnemyDeathParticle");
	}
}

void PlayScene::RespawnEnemiesIfCleared() {
	for (auto& obj : objects_) {
		if (obj->tag == GameTags::kEnemy) return; // まだ生存している敵がいるので何もしない
	}

	// タグ"EnemySpawner"のGameObjectにReflexEnemySpawnerComponentを付けておくと、
	// Inspectorで設定した「タグ＋出現数」のリストがここで反映される。見つからない場合は何も出さない
	GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner);
	if (!spawner) return;
	auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>();
	if (!config) return;

	// このリスポーン処理1回分のグリッドを最初に1度だけ生成し、スポーンする敵全員で
	// 使い回す（消費型）。敵ごとに違うセルが割り当てられるため、要求数がグリッドの
	// 容量以内である限り敵同士は絶対に重ならない
	std::vector<Vector3> cells = BuildShuffledSpawnGrid();

	for (const auto& entry : config->spawnEntries) {
		for (int i = 0; i < entry.count; i++) {
			SpawnEnemyAt(PickEnemySpawnPosition(cells), entry.tag);
		}
	}
}

void PlayScene::BeginPreparingPhase() {
	// pendingRespawnTags_（実行フェーズ中に倒した敵の種類の一覧）をそのままキューへ移す。
	// 消費順に意味は無いためシャッフルはしない（間隔を空けて1体ずつ出すだけの演出）
	respawnQueue_ = pendingRespawnTags_;
	pendingRespawnTags_.clear();
	respawnTimer_ = 0.0f;

	// 倒した敵が1体もいなかった場合（何もせず実行フェーズを終えた等）は補充するものが無いため、
	// 準備フェーズの演出をスキップして即座に計画フェーズへ戻す
	if (respawnQueue_.empty()) {
		if (auto* reflexPlayer = GetReflexPlayer()) {
			reflexPlayer->FinishPreparing();
		}
	}
}

void PlayScene::UpdatePreparingPhase(float deltaTime) {
	if (respawnQueue_.empty()) return;

	// 補充間隔はEnemySpawnerのInspectorで調整できるrespawnIntervalを使う。
	// EnemySpawnerが見つからない場合のみ既定値kRespawnIntervalにフォールバックする
	float interval = kRespawnInterval;
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			interval = config->respawnInterval;
		}
	}

	respawnTimer_ += deltaTime;
	if (respawnTimer_ < interval) return;
	respawnTimer_ = 0.0f;

	// 呼び出しのたびにグリッドを作り直す：直前にスポーンした敵が占有したマスを
	// 次の1体が避けられるようにするため（BuildShuffledSpawnGridは現在生存中の敵の
	// 座標を毎回見て占有マスを除外している）
	std::vector<Vector3> cells = BuildShuffledSpawnGrid();
	SpawnEnemyAt(PickEnemySpawnPosition(cells), respawnQueue_.back());
	respawnQueue_.pop_back();

	// このタイミングで最後の1体を出し終えたなら、準備フェーズを終えて計画フェーズへ戻す
	if (respawnQueue_.empty()) {
		if (auto* reflexPlayer = GetReflexPlayer()) {
			reflexPlayer->FinishPreparing();
		}
	}
}

std::vector<Vector3> PlayScene::BuildShuffledSpawnGrid() {
	// 現在生存している敵・プレイヤーが占有しているマスは候補から除外する。ここを素通りして
	// フィールド全体を毎回無条件に返していたため、常時湧きの1体補充のたびに「既に敵/プレイヤーが
	// いるマス」が候補に混ざり、偶然重なってスポーンすることがあった
	// （グリッドは「今空いているマス」だけを表すべき）。
	//
	// 閾値は占有元の種類で使い分ける：敵はグリッド座標そのもので生成されている（過去のスポーンで
	// 必ず格子点ぴったりに置かれている）ため、セル半分の狭い閾値で正確に検出できる。一方
	// プレイヤーは自由に移動する任意の座標（格子点からズレているのが常態）のため、狭い閾値では
	// すぐ近くの格子点しか弾けず、1〜2マス離れた「見た目にはまだ近い」マスに湧いてしまっていた。
	// プレイヤーの除外半径は「安全に離れていてほしい距離」であるkMinSpawnDistanceをそのまま使う
	struct OccupiedPoint { Vector3 pos; float radius; };
	std::vector<OccupiedPoint> occupied;
	for (auto& obj : objects_) {
		if (obj->tag != GameTags::kEnemy) continue;
		// SpawnMoveComponentが付いていて演出中（finished==false）の敵は、現在位置がまだ
		// startPos→targetPosの移動途中にあり、本来の着地マスにいない。ここで現在位置だけを
		// 占有マスとして報告すると、その敵の「本来の着地マス」がまだ空いていると誤判定され、
		// 演出完了後に別の敵と同じマスへ重複してスポーンしてしまっていた。演出中はtargetPos
		// （最終的にそのマスへ収まる位置）を占有マスとして扱うことでこれを防ぐ
		if (auto* spawnMove = obj->GetComponent<SpawnMoveComponent>()) {
			if (!spawnMove->finished) {
				occupied.push_back({ spawnMove->targetPos, kSpawnGridCellSize * 0.5f });
				continue;
			}
		}
		occupied.push_back({ obj->GetTransform().translation, kSpawnGridCellSize * 0.5f });
	}
	if (GameObject* player = FindObjectByTag(GameTags::kPlayer)) {
		occupied.push_back({ player->GetTransform().translation, kMinSpawnDistance });
	}
	auto isOccupied = [&](const Vector3& cell) {
		for (const OccupiedPoint& occ : occupied) {
			if (VectorMath::Length(cell - occ.pos) < occ.radius) return true;
		}
		return false;
	};

	std::vector<Vector3> cells;
	for (float x = kSpawnRangeMin; x <= kSpawnRangeMax; x += kSpawnGridCellSize) {
		for (float y = kSpawnRangeMin; y <= kSpawnRangeMax; y += kSpawnGridCellSize) {
			Vector3 cell{ x, y, 0.0f };
			if (!isOccupied(cell)) cells.push_back(cell);
		}
	}
	static std::mt19937 rng{ std::random_device{}() };
	std::shuffle(cells.begin(), cells.end(), rng);
	return cells;
}

Vector3 PlayScene::PickEnemySpawnPosition(std::vector<Vector3>& cells) {
	static std::mt19937 rng{ std::random_device{}() };

	// グリッドに空きがある間は、シャッフル済みの末尾から1つ取り出して消費する
	// （BuildShuffledSpawnGridが生存中の敵・プレイヤーのマスを除外済みのため、
	// ここで取り出す座標は誰とも重ならないことが保証されている）
	if (!cells.empty()) {
		Vector3 candidate = cells.back();
		cells.pop_back();
		return candidate;
	}

	// グリッドを使い切った場合（要求数がフィールド容量を超えた場合）は、従来通り
	// kMinSpawnDistance以上離れた地点をランダム再抽選するフォールバックに切り替える
	std::uniform_real_distribution<float> dist(kSpawnRangeMin, kSpawnRangeMax);
	Vector3 candidate{ 0.0f, 0.0f, 0.0f };
	for (int attempt = 0; attempt < kMaxSpawnAttempts; attempt++) {
		candidate = { dist(rng), dist(rng), 0.0f };

		bool tooClose = false;
		if (GameObject* player = FindObjectByTag(GameTags::kPlayer)) {
			if (VectorMath::Length(candidate - player->GetTransform().translation) < kMinSpawnDistance) tooClose = true;
		}
		if (!tooClose) {
			for (auto& obj : objects_) {
				if (obj->tag != GameTags::kEnemy) continue;
				if (VectorMath::Length(candidate - obj->GetTransform().translation) < kMinSpawnDistance) {
					tooClose = true;
					break;
				}
			}
		}
		if (!tooClose) return candidate;
	}
	return candidate; // 上限回数まで条件を満たせなかった場合は最後の候補をそのまま使う
}

PlayScene::TemplateShape PlayScene::DetermineTemplateShape(GameObject& templateObj) {
	if (templateObj.GetComponent<SphereRenderComponent>()) return TemplateShape::kSphere;
	if (templateObj.GetComponent<TriangleRenderComponent>()) return TemplateShape::kTriangle;
	return TemplateShape::kCube;
}

void PlayScene::SpawnEnemyAt(const Vector3& position, const std::string& templateTag) {
	EnemyTemplateData data = ReadEnemyTemplateData(templateTag);
	BuildEnemyFromTemplateData(position, templateTag, data);
}

PlayScene::EnemyTemplateData PlayScene::ReadEnemyTemplateData(const std::string& templateTag) {
	// templateTagを持つテンプレートGameObjectから見た目（形状＋色）・当たり判定（形状＋サイズ）・
	// ReflexEnemyComponentのパラメータを複製する。見つからない場合は赤い立方体
	// （既定サイズ・既定パラメータ）にフォールバックする（EnemyTemplateDataのメンバ初期化子が
	// そのままフォールバック値になる。sizeだけはkFallbackHalfSizeという名前付き定数を使う）
	EnemyTemplateData data;
	data.size = kFallbackHalfSize;

	if (GameObject* templateObj = FindObjectByTag(templateTag)) {
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
		if (auto* templateSpawnMove = templateObj->GetComponent<SpawnMoveComponent>()) {
			data.hasSpawnMove = true;
			data.spawnMoveZOffset = templateSpawnMove->zOffset;
			data.spawnMoveDuration = templateSpawnMove->duration;
			data.spawnMoveEasing = templateSpawnMove->easing;
		}
	}

	return data;
}

void PlayScene::BuildEnemyFromTemplateData(const Vector3& position, const std::string& templateTag, const EnemyTemplateData& data) {
	GameObject& enemy = CreateObject("Enemy");
	enemy.tag = GameTags::kEnemy;
	enemy.GetTransform().translation = position;
	// ヒエラルキーが敵だらけでフラットに埋まらないよう、"Enemies"フォルダの子としてぶら下げる
	// （フォルダはTransformが原点固定のため、子のtranslationはそのままワールド座標として扱われる）
	enemy.SetParent(&GetOrCreateGroupFolder(kEnemyFolderTag));

	if (data.hasSpawnMove) {
		// 本来のスポーン地点(position)をtargetPos、そこからZ方向にzOffset離れた地点をstartPosにする。
		// translationはstartPosから始め、SpawnMoveComponent::Updateが毎フレームtargetPosへ近づける
		auto* spawnMove = enemy.AddComponent<SpawnMoveComponent>();
		spawnMove->targetPos = position;
		spawnMove->startPos = position + Vector3{ 0.0f, 0.0f, data.spawnMoveZOffset };
		spawnMove->duration = data.spawnMoveDuration;
		spawnMove->easing = data.spawnMoveEasing;
		spawnMove->elapsed = 0.0f;
		spawnMove->finished = false; // SpawnMoveComponentの既定値はtrue（保存シーン安全側）のため、
		                             // 動的スポーン時はここで明示的にfalseへ戻して演出を開始する
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
	// 作り直す（AttachTextureAsset同様、コンストラクタ引数必須のためComponentRegistry::Create経由）
	if (!data.textureName.empty()) {
		ComponentLoadContext ctx = MakeComponentLoadContext();
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
			// テンプレートの固定速度ではなく、このスポーン個体専用にXYZ軸・速度・回転方向を
			// 毎回引き直す（テンプレート自体の値は変えず、複製先のインスタンスだけを乱数化する）
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
		// TextureSelectorComponent復元（ComponentRegistration.cpp）と同じく、名前から
		// projectAudioClips_内の現在のインデックスを探し直す
		int audioIndex = -1;
		for (size_t i = 0; i < projectAudioClips_.size(); i++) {
			if (projectAudioClips_[i].displayName == data.hitSoundAudioName) { audioIndex = static_cast<int>(i); break; }
		}
		enemy.AddComponent<HitSoundComponent>(&projectAudioClips_, audioIndex, data.hitSoundVolume);
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

	RebuildDerivedLists(); // gizmoTargets_に新規オブジェクトを反映する
}

void PlayScene::SpawnParticleBurstAt(const Vector3& position, const std::string& templateTag) {
	GameObject* templateObj = FindObjectByTag(templateTag);
	if (!templateObj) return; // テンプレートが無ければ演出なしで諦める（EnemySpawnerと違い必須要素ではない）

	auto* emitterConfig = templateObj->GetComponent<ParticleEmitterComponent>();
	if (!emitterConfig) return;

	// 見た目：テンプレートの具体型を判定して形状を決め、共通基底（color等）から色を取る
	// （SpawnEnemyAtの見た目複製と同じロジック。判定自体はDetermineTemplateShapeを共有する）
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
	GameObject& folder = GetOrCreateGroupFolder(kParticleFolderTag);

	for (int i = 0; i < emitterConfig->count; i++) {
		// 球面上の一様ランダム方向（z軸を一様抽選し、その高さの円周上をthetaで一様抽選する
		// 標準的な手法。緯度経度を直接一様抽選すると極付近に偏るため使わない）
		float z = zDist(rng);
		float theta = thetaDist(rng);
		float r = std::sqrt((std::max)(0.0f, 1.0f - z * z));
		Vector3 direction = { r * std::cos(theta), r * std::sin(theta), z };

		GameObject& particle = CreateObject("Particle");
		// excludeFromGizmoList=trueにはしない：GameObject::UpdateはSceneBase::Renderが
		// gizmoTargets_（RebuildDerivedListsがexcludeFromGizmoListを除外して構築する）を
		// イテレートして呼ぶため、trueにすると移動・サイズ変化・寿命判定（ParticleComponent::Update）
		// が一切実行されず消えなくなる。3Dクリックでの誤選択だけexcludeFromPickingで防ぐ
		particle.excludeFromPicking = true;
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
			// RotatorComponent::Randomizeで軸・速度・回転方向を粒子ごとに個別抽選する
			// （RotatorComponentを敵にランダム付与するときと同じ仕組みを流用）
			auto* rotator = particle.AddComponent<RotatorComponent>();
			rotator->randomizeOnSpawn = true;
			rotator->randomSpeedMin = emitterConfig->rotationSpeedMin;
			rotator->randomSpeedMax = emitterConfig->rotationSpeedMax;
			rotator->Randomize();
		}
	}

	RebuildDerivedLists(); // gizmoTargets_に新規オブジェクトを反映する
}

REGISTER_SCENE(PlayScene, "Play");
