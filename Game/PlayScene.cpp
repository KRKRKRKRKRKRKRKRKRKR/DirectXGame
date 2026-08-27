#include "PlayScene.h"
#include "GameTags.h"
#include "GameSession.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Math/VectorMath.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace {
	// フィールド範囲・敵同士の最小距離・グリッドセル間隔は、EnemySpawner
	// （ReflexEnemySpawnerComponent::spawnRangeMinX/MaxX/MinY/MaxY、minSpawnDistance、
	// spawnGridCellSize）がInspectorから調整できる（X/Y独立の値、ReflexPlayerComponent::
	// fieldRangeMinX/MaxX/MinY/MaxYと同じ方式）。以下はEnemySpawnerが見つからない場合のみ使う
	// フォールバック定数（壁が±10付近にある想定で、壁の厚み・敵自身の半径分の余裕を持たせた内側±8。
	// X/Y共通のフォールバック値のみ用意し、EnemySpawnerが見つかればX/Y独立の値で上書きされる）
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

	// 残りラウンド数がこの値以下になったらRoundCountAlphabetの警告演出（赤色固定＋パルス拡大）を
	// 開始する閾値
	constexpr int kRoundWarningThreshold = 5;

	// 警告演出の色（常に赤固定）
	constexpr Vector4 kRoundWarningColor = { 1.0f, 0.15f, 0.15f, 1.0f };

	// パルス（ReflexPlayerComponentの移動先マーカーの波紋演出と同じ「最小サイズから広がって
	// 最大サイズに達したら、また最小サイズから再発生する」片道の繰り返し）1周期の長さ(秒)。
	// 残りラウンド数が少ないほど周期を短くして切迫感を出す：
	// period = kPulsePeriodBase + kPulsePeriodPerRound * remaining
	// （remaining=5で1.0秒、remaining=1で0.36秒、remaining=0で0.2秒程度になる）
	constexpr float kPulsePeriodBase = 0.2f;
	constexpr float kPulsePeriodPerRound = 0.16f;

	// パルスのスケール範囲（1.0倍を基準に、最小〜最大の倍率）
	constexpr float kPulseMinScale = 1.0f;
	constexpr float kPulseMaxScale = 1.3f;
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
	ImGui::DragFloat("スポーン間隔の最小（秒）", &spawnerConfig->spawnIntervalMin, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("スポーン間隔の最大（秒）", &spawnerConfig->spawnIntervalMax, 0.01f, 0.0f, 5.0f);

	ImGui::Separator();
	ImGui::Text("スポーン時のサイズランダム化（テンプレートに対する倍率）");
	ImGui::DragFloat("最小倍率", &spawnerConfig->sizeScaleMin, 0.02f, 0.1f, 5.0f);
	ImGui::DragFloat("最大倍率", &spawnerConfig->sizeScaleMax, 0.02f, 0.1f, 5.0f);

	ImGui::Separator();
	ImGui::DragFloat("プレイヤーとの最小距離", &spawnerConfig->playerExclusionRadius, 0.1f, 0.0f, 20.0f);
	// 最大が最小を下回ると乱数範囲が壊れる（std::uniform_real_distributionが未定義動作になる）ため、
	// UI操作直後に矛盾した値になった場合はここで最小側へ揃えて防ぐ
	if (spawnerConfig->sizeScaleMax < spawnerConfig->sizeScaleMin) {
		spawnerConfig->sizeScaleMax = spawnerConfig->sizeScaleMin;
	}

	ImGui::Separator();
	ImGui::Text("敵のスポーン範囲（壁の内側に収まるよう設定すること）");
	ImGui::DragFloat("範囲 X最小", &spawnerConfig->spawnRangeMinX, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat("範囲 X最大", &spawnerConfig->spawnRangeMaxX, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat("範囲 Y最小", &spawnerConfig->spawnRangeMinY, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat("範囲 Y最大", &spawnerConfig->spawnRangeMaxY, 0.1f, -100.0f, 100.0f);
	if (spawnerConfig->spawnRangeMaxX < spawnerConfig->spawnRangeMinX) {
		spawnerConfig->spawnRangeMaxX = spawnerConfig->spawnRangeMinX;
	}
	if (spawnerConfig->spawnRangeMaxY < spawnerConfig->spawnRangeMinY) {
		spawnerConfig->spawnRangeMaxY = spawnerConfig->spawnRangeMinY;
	}
	ImGui::DragFloat("敵同士の最小距離", &spawnerConfig->minSpawnDistance, 0.05f, 0.0f, 20.0f);
	ImGui::DragFloat("スポーン用グリッドのセル間隔", &spawnerConfig->spawnGridCellSize, 0.05f, 0.1f, 20.0f);

	ImGui::Separator();
	ImGui::Text("クリア条件");
	ImGui::DragInt("目標ラウンド数", &spawnerConfig->totalRounds, 0.2f, 1, 999);
}

std::vector<std::pair<std::string, SceneBase::HudDefinition>> PlayScene::BuildHudDefinitions() {
	// 基底（Camera Coord/FPS）に、PlayScene固有の3つ（実行タイマー・撃破数・コンボ）を追加する
	auto definitions = SceneBase::BuildHudDefinitions();

	definitions.push_back({ "Execution Timer", HudDefinition{ 256, 64, [this]() {
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Time: %.1f", executionTimer_);
		return std::string(buf);
	} } });
	definitions.push_back({ "Kill Count", HudDefinition{ 256, 64, [this]() {
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Kills: %d", killCount_);
		return std::string(buf);
	} } });
	definitions.push_back({ "Combo Count", HudDefinition{ 256, 64, [this]() {
		// このHUDは連続撃破数（comboCount_）そのものではなく、現在の実行フェーズ中に
		// 稼いだスコアの累計（phaseScore_）を表示する
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Combo: %d", phaseScore_);
		return std::string(buf);
	} } });
	definitions.push_back({ "Score", HudDefinition{ 256, 64, [this]() {
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Score: %d", score_);
		return std::string(buf);
	} } });

	return definitions;
}

void PlayScene::OnInitialize() {
	// Camera座標を毎フレーム表示するHUD。表示内容はTextProviderとして1回登録するだけで、
	// 以降はSceneBase::Render内の汎用ループが毎フレーム自動的にUpdateDynamicText()を呼んでくれる
	CreateHud("Camera Coord");

	// 実行タイマー・撃破数・コンボ数・スコアのHUDはここでは自動生成しない。BuildHudDefinitions()に
	// エントリを追加済みのため、Inspectorの「Objects」パネル→選択中オブジェクトのTextRender
	// 「HUD種別」コンボに"Execution Timer"/"Kill Count"/"Combo Count"/"Score"として現れる。
	// 必要な人が必要なタイミングで手動追加できるようにするため、自動生成は行わない。
	// 残りラウンド数はKillCountAlphabetと同じ運用（タグ"RoundCountAlphabet"のAlphabetTextComponent
	// にHandleSceneTransitionInputがTextProviderを紐付ける）のため、ここでも何もしない

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
	// 読み込まれていない。ここでResetAllEnemies()を呼んでもFindObjectByTagが
	// 何も見つけられず常に空振りする（初回スポーンが出ない原因だった）。
	// 実際の初回スポーンはHandleSceneTransitionInput側の初回フレーム判定（needsInitialSpawn_）で行う
}

ReflexPlayerComponent* PlayScene::GetReflexPlayer() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	if (!player) return nullptr;
	return player->GetComponent<ReflexPlayerComponent>();
}

ComboPopupComponent* PlayScene::GetComboPopup() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	if (!player) return nullptr;
	return player->GetComponent<ComboPopupComponent>();
}

int PlayScene::GetTotalRounds() {
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			return config->totalRounds;
		}
	}
	return 30;
}

void PlayScene::HandleSceneTransitionInput() {
	// 初回フレームのみ：OnInitialize()の時点ではまだLoadScene()が済んでおらず
	// EnemySpawner/テンプレートが存在しないため、ここ（LoadScene()完了後に必ず呼ばれる
	// 最初のフレーム）で初回スポーンを行う。ResetAllEnemies()は保存シーンJSONに残っていた敵を
	// 問答無用で全部消してから配置し直すため、Playシーンを開くたびに毎回敵構成がリセットされる
	if (needsInitialSpawn_) {
		needsInitialSpawn_ = false;
		ResetAllEnemies();

		// タグ"KillCountAlphabet"のGameObjectにAlphabetTextComponentが付いていれば、
		// killCount_を文字列化するTextProviderを紐付ける（ユーザーがInspectorで
		// AlphabetTextComponentを追加し、タグだけ付ければ自動でキルカウント表示になる）。
		// LoadScene()完了後のこのタイミングでのみ行えば十分（Providerはラムダとして
		// 保持され続けるため、毎フレーム呼び直す必要はない）
		if (GameObject* killCountObj = FindObjectByTag(GameTags::kKillCountAlphabet)) {
			if (auto* alphabetText = killCountObj->GetComponent<AlphabetTextComponent>()) {
				alphabetText->SetTextProvider([this]() {
					return std::to_string(killCount_);
				});
			}
		}

		// タグ"RoundCountAlphabet"のGameObjectにAlphabetTextComponentが付いていれば、
		// 残りラウンド数（GetTotalRounds() - roundCount_）を文字列化するTextProviderを紐付ける。
		// KillCountAlphabetと同じ運用（ユーザーがInspectorでタグを付けるだけで自動的に動く）
		if (GameObject* roundCountObj = FindObjectByTag(GameTags::kRoundCountAlphabet)) {
			if (auto* alphabetText = roundCountObj->GetComponent<AlphabetTextComponent>()) {
				alphabetText->SetTextProvider([this]() {
					int remaining = (std::max)(GetTotalRounds() - roundCount_, 0);
					return std::to_string(remaining);
				});
			}
		}

		// タグ"ScoreAlphabet"のGameObjectにAlphabetTextComponentが付いていれば、
		// 現在のスコア（score_）を文字列化するTextProviderを紐付ける。KillCountAlphabetと同じ運用
		if (GameObject* scoreObj = FindObjectByTag(GameTags::kScoreAlphabet)) {
			if (auto* alphabetText = scoreObj->GetComponent<AlphabetTextComponent>()) {
				// score_そのものではなくdisplayScore_（UpdateScoreDisplayが1ずつ近づける表示専用の値）
				// を文字列化する。加算された瞬間に一気に反映せず、一の位から連続でカウントアップ
				// して見えるようにするため
				alphabetText->SetTextProvider([this]() {
					return std::to_string(displayScore_);
				});
			}
		}
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
		UpdateExecutionPhaseStats(reflexPlayer, lastDeltaTime_);
	}

	// 残りラウンド数が少なくなったらRoundCountAlphabetを赤くパルスさせて警告する
	UpdateRoundCountWarningVisual(lastDeltaTime_);

	// ScoreAlphabetのHUD表示を、score_へ向けて1ずつカウントアップさせる
	UpdateScoreDisplay(lastDeltaTime_);

	// ESCでいつでもTitleへ戻れるようにする
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = "Title";

	// デバッグ用：F2でその場のスコアのままClearSceneへ直接遷移する（目標ラウンド数まで
	// 実際にプレイしなくても、Clear画面の見た目・演出だけをすぐ確認できるようにするため）
	if (Input::IsTriggered(DIK_F2)) {
		GameSession::GetInstance().SetScore(score_);
		nextScene_ = "Clear";
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
	// ループ内で毎回FindObjectByTagし直さないよう、1回だけ取得しておく（無ければnullptrのまま、
	// コンボ演出が単に出ないだけで撃破処理自体には影響しない）
	ComboPopupComponent* comboPopup = GetComboPopup();
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
				// HUD用の撃破数・コンボ数。killCount_は累計、comboCount_は計画フェーズへ戻るまで
				// 積み上がり続ける（UpdateExecutionPhaseStatsがフェーズ切替の瞬間に0へ戻す）
				killCount_++;
				comboCount_++;

				// スコアは「1体倒すごとに現在のコンボ数を加算」する方式。comboCount_を先に
				// インクリメントした後の値を使うため、同じ実行フェーズ内で連続して倒すほど
				// 1体あたりの獲得スコアが増えていく（コンボを繋げることへの報酬になる）
				score_ += comboCount_;
				phaseScore_ += comboCount_;

				// プレイヤー頭上には、連続撃破数（comboCount_）そのものではなく、この実行フェーズ
				// 中に稼いだ累計スコア（phaseScore_、HUDのCombo欄と同じ値）をポップアップ表示する。
				// ComboPopupComponentが付いていない（プレイヤーに未追加）場合は何もしない
				if (comboPopup) comboPopup->RequestPopup(phaseScore_);
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

void PlayScene::ResetAllEnemies() {
	// tag=="Enemy"のオブジェクトを問答無用で全部削除する（保存シーンJSONから復元されていた分・
	// 前回のプレイの残り、どちらも対象）。「既に敵がいれば何もしない」ガードを設けず、
	// Playシーンを開くたびに毎回この関数で敵構成をリセットしたいため
	std::vector<GameObject*> existingEnemies;
	for (auto& obj : objects_) {
		if (obj->tag == GameTags::kEnemy) existingEnemies.push_back(obj.get());
	}
	if (!existingEnemies.empty()) DeleteObjects(existingEnemies);

	SpawnEnemiesFromConfig();
}

void PlayScene::SpawnEnemiesFromConfig() {
	// タグ"EnemySpawner"のGameObjectにReflexEnemySpawnerComponentを付けておくと、
	// Inspectorで設定した「タグ＋出現数」のリストがここで反映される。見つからない場合は何も出さない
	GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner);
	if (!spawner) return;
	auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>();
	if (!config) return;

	// spawnEntries（種類ごとの個数）を1体ずつのタグ列に展開し、種類も含めて丸ごとシャッフルする。
	// 一気に全部出さず、UpdatePreparingPhaseの「準備フェーズ＝1体ずつランダム間隔で出す」仕組みを
	// そのまま流用する（respawnQueue_はback()から1つずつpop_backで消費するので、シャッフル順が
	// そのままスポーン順になる）
	std::vector<std::string> initialQueue;
	for (const auto& entry : config->spawnEntries) {
		for (int i = 0; i < entry.count; i++) {
			initialQueue.push_back(entry.tag);
		}
	}
	if (initialQueue.empty()) return;

	static std::mt19937 rng{ std::random_device{}() };
	std::shuffle(initialQueue.begin(), initialQueue.end(), rng);

	respawnQueue_ = std::move(initialQueue);
	respawnTimer_ = 0.0f;
	currentSpawnInterval_ = PickNextSpawnInterval();

	// これから入るkPreparingは「実行フェーズ完了後の補充」ではなく起動直後の初回スポーンのため、
	// このkPreparing→kPlanning遷移をUpdateExecutionPhaseStatsのラウンドカウントに数えないよう
	// フラグを立てておく
	isInitialSpawnInProgress_ = true;

	// プレイヤーはこの時間差スポーンが終わるまで操作不可にする（実行フェーズ完了後の
	// 補充スポーンと同じ「準備フェーズ」を、計画フェーズから明示的に開始する形で流用する）
	if (auto* reflexPlayer = GetReflexPlayer()) {
		reflexPlayer->BeginPreparing();
	}
}

void PlayScene::UpdateScoreDisplay(float deltaTime) {
	if (displayScore_ >= score_) {
		// 追いつききっている間はタイマーを進めない（次の加算が起きた瞬間から
		// またkScoreCountUpInterval後にカウントアップを始めるようにするため）
		scoreCountUpTimer_ = 0.0f;
		return;
	}

	scoreCountUpTimer_ += deltaTime;
	while (scoreCountUpTimer_ >= kScoreCountUpInterval && displayScore_ < score_) {
		scoreCountUpTimer_ -= kScoreCountUpInterval;
		displayScore_++;
	}
}

void PlayScene::UpdateExecutionPhaseStats(ReflexPlayerComponent* reflexPlayer, float deltaTime) {
	ReflexPlayerComponent::Phase currentPhase = reflexPlayer->GetPhase();

	// 実行フェーズ中だけ経過秒数を積算する（計画フェーズ・準備フェーズ中は止めておく）
	if (currentPhase == ReflexPlayerComponent::Phase::kExecuting) {
		executionTimer_ += deltaTime;
	}

	// kPreparing→kPlanning（準備フェーズが終わって計画フェーズへ戻った瞬間）を検知したら、
	// 次の実行フェーズに備えてタイマー・コンボを0にリセットする。killCount_（累計）はリセットしない。
	// 敵を1体も倒さずに実行フェーズを終えた場合、BeginPreparingPhaseがrespawnQueue_が空のまま
	// 即座にFinishPreparing()を呼ぶため、kPreparingを1フレームも経由せずkExecuting→kPlanningへ
	// 直接遷移する。この場合も同じく1ラウンドの区切りとして扱う必要があるため、kExecuting→
	// kPlanningもここで検知対象に含める（さもないと「敵を倒さなかった回はラウンドが
	// 進まない」不具合になる）
	if ((previousPhase_ == ReflexPlayerComponent::Phase::kPreparing ||
		 previousPhase_ == ReflexPlayerComponent::Phase::kExecuting) &&
		currentPhase == ReflexPlayerComponent::Phase::kPlanning) {
		executionTimer_ = 0.0f;
		comboCount_ = 0;
		phaseScore_ = 0;

		// 実行フェーズ終了に合わせて、表示中のコンボポップアップも演出の途中でも即座に消す
		if (auto* comboPopup = GetComboPopup()) {
			comboPopup->ClearAll();
		}

		// 「準備フェーズが終わり、計画フェーズに戻った瞬間」を1ラウンドの区切りとして数える。
		// ただし起動直後の初回スポーン完了によるこの遷移は「1ラウンドを実際にプレイし終えた」
		// わけではないため数えない（isInitialSpawnInProgress_のコメント参照）
		if (isInitialSpawnInProgress_) {
			isInitialSpawnInProgress_ = false;
		} else {
			// 目標ラウンド数に達したら、その時点のスコアをランキングに登録するためClearSceneへ遷移する
			roundCount_++;
			if (roundCount_ >= GetTotalRounds()) {
				// ClearSceneはGameObjectを持たないPlaySceneのメンバへ直接アクセスできないため、
				// シーンをまたいで値を持ち越せるGameSessionシングルトン経由で最終スコアを渡す
				GameSession::GetInstance().SetScore(score_);
				nextScene_ = "Clear";
			}
		}
	}

	previousPhase_ = currentPhase;
}

void PlayScene::UpdateRoundCountWarningVisual(float deltaTime) {
	GameObject* roundCountObj = FindObjectByTag(GameTags::kRoundCountAlphabet);
	if (!roundCountObj) return;
	auto* alphabetText = roundCountObj->GetComponent<AlphabetTextComponent>();
	if (!alphabetText) return;

	int remaining = (std::max)(GetTotalRounds() - roundCount_, 0);

	if (remaining > kRoundWarningThreshold) {
		// 閾値を上回っている間は既定の見た目（等倍・白）に戻し、タイマーも次に閾値以下へ
		// 入った瞬間に同じ位相（scale最小）から始まるようリセットしておく
		alphabetText->displayScaleMultiplier = 1.0f;
		alphabetText->displayColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		roundWarningPulseTimer_ = 0.0f;
		return;
	}

	// 残りが少ないほど周期を短くする（remaining=0のときが最速）
	float period = kPulsePeriodBase + kPulsePeriodPerRound * static_cast<float>(remaining);
	roundWarningPulseTimer_ = std::fmod(roundWarningPulseTimer_ + deltaTime, period);

	// ReflexPlayerComponent::DrawPlanningVisualizationの波紋アニメーションと同じ考え方：
	// t（0=周期の始まり、1=周期の終わり）をkInOutSineでイージングし、最小→最大サイズへ
	// 片道で広がったら、また最小サイズから再発生する（往復ではなく毎回リセットする波紋型）
	float t = period > 0.0f ? roundWarningPulseTimer_ / period : 0.0f;
	float eased = Easing::Apply(Easing::Type::kInOutSine, t);
	alphabetText->displayScaleMultiplier = kPulseMinScale + (kPulseMaxScale - kPulseMinScale) * eased;
	alphabetText->displayColor = kRoundWarningColor;
}

float PlayScene::PickNextSpawnInterval() {
	float intervalMin = kRespawnInterval;
	float intervalMax = kRespawnInterval;
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			intervalMin = config->spawnIntervalMin;
			intervalMax = config->spawnIntervalMax;
		}
	}
	// maxがminを下回っているとstd::uniform_real_distributionが未定義動作になるため、
	// Inspector側のガードをすり抜けて不正な値が来た場合の保険として上限を下限でクランプする
	// （PlayScene::SpawnEnemyAtのsizeScaleMin/Max抽選と同じパターン）
	intervalMax = (std::max)(intervalMax, intervalMin);
	static std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist(intervalMin, intervalMax);
	return dist(rng);
}

void PlayScene::BeginPreparingPhase() {
	// pendingRespawnTags_（実行フェーズ中に倒した敵の種類の一覧）をそのままキューへ移す。
	// 消費順に意味は無いためシャッフルはしない（間隔を空けて1体ずつ出すだけの演出）
	respawnQueue_ = pendingRespawnTags_;
	pendingRespawnTags_.clear();
	respawnTimer_ = 0.0f;
	currentSpawnInterval_ = PickNextSpawnInterval();

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

	respawnTimer_ += deltaTime;
	if (respawnTimer_ < currentSpawnInterval_) return;
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
	} else {
		// まだ残りがある場合は次の1体分の待ち時間を新しく抽選する
		currentSpawnInterval_ = PickNextSpawnInterval();
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
	// プレイヤーの除外半径は「安全に離れていてほしい距離」であるplayerExclusionRadiusをそのまま使う。
	// スポーン範囲・グリッドセル間隔もEnemySpawner（Inspectorで調整可能）から取得し、
	// 見つからない場合のみkSpawnRangeMin/Max・kSpawnGridCellSizeにフォールバックする
	float spawnRangeMinX = kSpawnRangeMin;
	float spawnRangeMaxX = kSpawnRangeMax;
	float spawnRangeMinY = kSpawnRangeMin;
	float spawnRangeMaxY = kSpawnRangeMax;
	float gridCellSize = kSpawnGridCellSize;
	float exclusionRadius = kMinSpawnDistance;
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
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
	for (auto& obj : objects_) {
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
	if (GameObject* player = FindObjectByTag(GameTags::kPlayer)) {
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
	// minSpawnDistance以上離れた地点をランダム再抽選するフォールバックに切り替える。
	// スポーン範囲・敵同士の最小距離・プレイヤーとの間隔はEnemySpawner（Inspectorで調整可能）
	// から取得し、見つからない場合のみkSpawnRangeMin/Max・kMinSpawnDistanceにフォールバックする
	float spawnRangeMinX = kSpawnRangeMin;
	float spawnRangeMaxX = kSpawnRangeMax;
	float spawnRangeMinY = kSpawnRangeMin;
	float spawnRangeMaxY = kSpawnRangeMax;
	float minSpawnDistance = kMinSpawnDistance;
	float playerExclusionRadius = kMinSpawnDistance;
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
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
		if (GameObject* player = FindObjectByTag(GameTags::kPlayer)) {
			if (VectorMath::Length(candidate - player->GetTransform().translation) < playerExclusionRadius) tooClose = true;
		}
		if (!tooClose) {
			for (auto& obj : objects_) {
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

PlayScene::TemplateShape PlayScene::DetermineTemplateShape(GameObject& templateObj) {
	if (templateObj.GetComponent<SphereRenderComponent>()) return TemplateShape::kSphere;
	if (templateObj.GetComponent<TriangleRenderComponent>()) return TemplateShape::kTriangle;
	return TemplateShape::kCube;
}

void PlayScene::SpawnEnemyAt(const Vector3& position, const std::string& templateTag) {
	EnemyTemplateData data = ReadEnemyTemplateData(templateTag);

	// EnemySpawnerのsizeScaleMin/sizeScaleMaxの範囲でランダムな倍率を抽選する。
	// EnemySpawnerが見つからない場合は等倍（1.0）のまま
	// （UpdatePreparingPhaseがrespawnIntervalを読むのと同じ「見つからなければ既定動作」パターン）。
	// data.sizeそのものは変更しない（BuildEnemyFromTemplateData/EnemyTemplateData::sizeScaleの
	// コメント参照：halfSizeとscaleの二重適用を避けるため）
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			// maxがminを下回っているとstd::uniform_real_distributionが未定義動作になるため、
			// Inspector側のガードをすり抜けて不正な値が来た場合の保険として上限を下限でクランプする
			float scaleMin = config->sizeScaleMin;
			float scaleMax = (std::max)(config->sizeScaleMax, scaleMin);
			static std::mt19937 rng{ std::random_device{}() };
			std::uniform_real_distribution<float> dist(scaleMin, scaleMax);
			data.sizeScale = dist(rng);
		}
	}

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
	}

	return data;
}

void PlayScene::BuildEnemyFromTemplateData(const Vector3& position, const std::string& templateTag, const EnemyTemplateData& data) {
	GameObject& enemy = CreateObject("Enemy");
	enemy.tag = GameTags::kEnemy;
	enemy.GetTransform().translation = position;
	// スポーン時のサイズランダム化（PlayScene::SpawnEnemyAt、EnemySpawner::sizeScaleMin/Max）を
	// Transform.scaleに適用する。data.size（コライダーのhalfSize/radius）自体はテンプレート本来の
	// 値のまま変更しない：OBB/SphereColliderComponentは「halfSize（絶対値）× Transform.scale」で
	// ワールド判定サイズを出す設計のため（GetWorldOBB参照）、ここでscaleにsizeScaleを掛けることで
	// 見た目・当たり判定の両方が同じ倍率で連動して拡縮される（data.size側では掛けない＝二重適用を防ぐ）
	enemy.GetTransform().scale = { data.sizeScale, data.sizeScale, data.sizeScale };
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

	if (data.hasSpawnSound && !data.spawnSoundAudioName.empty()) {
		int audioIndex = -1;
		for (size_t i = 0; i < projectAudioClips_.size(); i++) {
			if (projectAudioClips_[i].displayName == data.spawnSoundAudioName) { audioIndex = static_cast<int>(i); break; }
		}
		auto* spawnSound = enemy.AddComponent<SpawnSoundComponent>(&projectAudioClips_, audioIndex, data.spawnSoundVolume);
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
	// フォルダ自体も中身（パーティクル）と同じく実行時にしか意味を持たない一時的なコンテナのため
	// 保存対象外にする（GetOrCreateGroupFolderは"Enemies"フォルダにも使う共通関数のため、
	// この除外設定はここで個別に行う）
	folder.excludeFromSave = true;

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
		// lifeTime経過で自動的に消える一時的なGameObjectのため、セーブ操作のタイミングと重なって
		// たまたま保存されてしまわないよう明示的に保存対象外にする（ParticleComponent自体は
		// ComponentRegistryに未登録＝元々保存対象外だが、見た目のRenderComponentBase側はGameObjectの
		// 一部として保存されてしまうため、GameObject単位で除外する必要がある）
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
