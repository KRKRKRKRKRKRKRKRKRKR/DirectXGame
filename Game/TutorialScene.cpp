#include "TutorialScene.h"
#include "GameTags.h"
#include "FadeManager.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/Component/Physics/ReflexEnemyComponent.h"
#include "../Engine/GameObject/Component/Physics/SpawnMoveComponent.h"
#include "../Engine/GameObject/Component/Render/AlphabetTextComponent.h"
#include "../Engine/GameObject/Component/Render/ClickHintMarkerComponent.h"
#include "../Engine/GameObject/Component/Physics/OBBColliderComponent.h"
#include "../Engine/GameObject/Component/Physics/PlayButtonComponent.h"
#include "../Math/Easing.h"

namespace {
	// ClearScene::PlayBackspaceExitAnimationのkExitZOffset/kExitDurationと同じ値
	// （手前から奥へ縮小しながら消える退場演出の距離・時間）
	constexpr float kHintExitZOffset = 6.0f;
	constexpr float kHintExitDuration = 0.35f;

	// tag=kTutorialHintAlphabetのGameObjectに表示する操作説明文言。scene.json側のtextは
	// 空にしておき、フェードが完全に終わった瞬間にこの文字列を流し込む（ShowHintIfFadeFinished）
	constexpr const char* kHintText = "Click to move";

	// PlayScene::kTemplateHideYと同じ値。退場演出が終わったヒントテキストをDeleteObjectsで
	// 消さず、フィールド外のここへ退避させることで隠す。DeleteObjectsだとGameObject自体が
	// scene.jsonから消えてしまい、演出後にシーン保存すると次回以降ヒントが二度と出なくなる
	// バグになっていたため（削除ではなく退避に変更した経緯）
	constexpr float kHintHideY = -1000.0f;

	// チュートリアルの敵4体の固定スポーン座標（上下左右）。scene.json側にGameObjectとして
	// 保存させず、毎回このコード上の座標からSpawnEnemyAtで再生成する。手動配置した敵を
	// 実機でプレイして全滅させた状態のまま保存すると、DeleteObjectsされた実体が
	// scene.jsonから消えて二度と復活しないバグ（ヒントテキストと同種の問題）を
	// 起こしていたため、GameObjectの永続化自体をやめて「毎回作り直す」方式に変更した
	constexpr Vector3 kEnemySpawnPositions[] = {
		{  7.0f,  0.0f, 0.0f },
		{ -7.0f,  0.0f, 0.0f },
		{  0.0f,  7.0f, 0.0f },
		{  0.0f, -7.0f, 0.0f },
	};

	// 敵テンプレートのタグ名。ReflexEnemySpawnerComponent::spawnEntriesの既定値と同じ
	// "AEnemy"を使う（Resources/Tutorial/scene.jsonの敵テンプレート）
	constexpr const char* kEnemyTemplateTag = "AEnemy";

	// クリック誘導マーカー（ReflexPlayerComponentの経路予約マーカーと同じ見た目のCircle.obj、
	// 波紋パルスアニメーション付き）を表示する固定5地点（四隅＋最後にもう一度右上）。
	// 敵の初期スポーン位置（kEnemySpawnPositions、上下左右）とは別に、フィールドの四隅を
	// クリックできることを示す目印として使う。右上→左上→左下→右下→右上の順に1つずつ表示し、
	// その座標がクリック（経路予約）されたら次の座標へイージングで移動する（TutorialScene::
	// AdvanceClickHintIfClicked）。末尾（2度目の右上）がクリックされたら、循環せずそこで停止する。
	// "Click"の文字ラベルは最初（1度目の右上）だけ表示し、それ以降はサークルのみになる
	constexpr Vector3 kClickHintMarkerPositions[] = {
		{  7.0f,  7.0f, 0.0f }, // 右上
		{ -7.0f,  7.0f, 0.0f }, // 左上
		{ -7.0f, -7.0f, 0.0f }, // 左下
		{  7.0f, -7.0f, 0.0f }, // 右下
		{  7.0f,  7.0f, 0.0f }, // 右上（終了地点）
	};

	// マーカーの上に表示する"Click"テキストの、マーカー中心からのYオフセット
	constexpr float kClickHintLabelYOffset = 1.2f;

	// マーカーが次の地点へ移動する際のイージング演出の所要時間・種類
	constexpr float kClickHintMoveDuration = 0.4f;
	constexpr Easing::Type kClickHintMoveEasing = Easing::Type::kOutCubic;

	// ラベル（"Click"テキスト）を隠す際のY座標退避先。kHintHideYと同じ値・同じ理由
	// （DeleteObjectsではなく非表示化。詳細はkHintHideYのコメント参照）
	constexpr float kClickHintLabelHideY = -1000.0f;
}

void TutorialScene::HandleSceneTransitionInput() {
	// 初回フレームのみ：既存のtag=Enemy実体（前回Playで倒され損なった、あるいは保存時点の
	// 中途半端な状態がscene.jsonに残っている場合がある）を全部消してから、kEnemySpawnPositions
	// の固定4座標へSpawnEnemyAtで再配置する。EnemySpawnerによる自動ランダムスポーンは使わない。
	// GameObjectとしての永続化に頼らず「毎回コード上の座標から作り直す」方式にすることで、
	// 実機で全滅させた状態のまま保存しても次回起動時に必ず4体へ戻るようにしている
	// （scene.jsonにtag=Enemyを直接置く方式だと、削除された状態のまま保存されて
	// 二度と復活しないバグになっていたため）
	if (needsInitialSpawn_) {
		needsInitialSpawn_ = false;

		// Playerの初期座標は常に原点固定にする。実機でギズモ移動したままシーンを保存すると
		// scene.json側のtranslationがズレたままになる（敵やヒントテキストと同種の問題）ため、
		// 起動のたびにコード側から強制的に0,0,0へ戻す
		if (GameObject* player = FindObjectByTag(GameTags::kPlayer)) {
			player->GetTransform().translation = { 0.0f, 0.0f, 0.0f };
			player->GetTransform().rotation = { 0.0f, 0.0f, 0.0f };
		}

		std::vector<GameObject*> existingEnemies;
		for (auto& obj : objects_) {
			if (obj->tag == GameTags::kEnemy) existingEnemies.push_back(obj.get());
		}
		if (!existingEnemies.empty()) DeleteObjects(existingEnemies);

		for (const Vector3& pos : kEnemySpawnPositions) {
			SpawnEnemyAt(pos, kEnemyTemplateTag);
		}

		SpawnClickHintMarkers();
	}

	// フェーズ管理（計画/実行/準備）・敵撃破処理・実行タイマー/コンボ更新は
	// PlaySceneのロジックをそのまま流用する。ここではPlayScene固有のESC/F1デバッグ遷移が
	// 走らないよう、末尾の遷移判定を自前で行うためPlayScene::HandleSceneTransitionInputは
	// 呼ばず、必要な部分だけ以下に複製する
	ProcessPendingDestroys();

	// Title→Tutorial遷移のフェードが完全に消え終わったら、操作説明テキストの登場演出を始める
	ShowHintIfFadeFinished();

	if (auto* reflexPlayer = GetReflexPlayer()) {
		if (reflexPlayer->ConsumeExecutionFinished()) {
			BeginPreparingPhase();
		}
		if (reflexPlayer->GetPhase() == ReflexPlayerComponent::Phase::kPreparing) {
			UpdatePreparingPhase(lastDeltaTime_);
		}
		UpdateExecutionPhaseStats(reflexPlayer, lastDeltaTime_);

		// プレイヤーが最初のクリックで経路を1点でも予約したら、操作説明テキスト
		// （tag=kTutorialHintAlphabet）の退場演出を1度だけ開始する。GetWaypointCount()>0は
		// 計画フェーズ中にクリックした瞬間から実行フェーズ完了までtrueであり続けるため、
		// hintRemoved_で「既に開始した」ことを覚えておき、二重に演出を開始しないようにする
		if (!hintRemoved_ && reflexPlayer->GetWaypointCount() > 0) {
			hintRemoved_ = true;
			StartHintExitAnimation();
		}
	}

	// クリック誘導マーカー自身のPlayButtonComponentがクリックされたかを毎フレーム確認する
	AdvanceClickHintIfClicked();

	// 退場演出（縮小しながら奥へ動く）が完了したヒントテキストを、フィールド外（kHintHideY）へ
	// 退避して隠す。DeleteObjectsは使わない（削除するとGameObject自体がscene.jsonから消え、
	// 演出後にシーンを保存すると次回以降ヒントが二度と出なくなってしまうため）
	if (GameObject* hint = FindObjectByTag(GameTags::kTutorialHintAlphabet)) {
		auto* spawnMove = hint->GetComponent<SpawnMoveComponent>();
		if (spawnMove && spawnMove->destroyOnFinish && spawnMove->finished) {
			hint->RemoveComponent<SpawnMoveComponent>();
			hint->GetTransform().translation.y = kHintHideY;
		}
	}

	// ESCでいつでもTitleへ戻れるようにする（PlaySceneと同じキー）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = "Title";

	// チュートリアル固有：配置された敵を全滅させたら自動でPlayScene本編へ進む。
	// isPlaying_（再生中）でない間は判定しない。停止中（Inspectorで敵を編集している間）は
	// 敵が0体の瞬間が普通にあり得るため、判定してしまうと編集のたびにPlayへ遷移要求が
	// 飛んでしまい編集作業ができない
	if (isPlaying_ && AreAllEnemiesDefeated()) {
		nextScene_ = "Play";
	}
}

void TutorialScene::SpawnClickHintMarkers() {
	// kClickHintMarkerPositions[0]（右上）にだけ、ReflexPlayerComponentの経路予約マーカーと
	// 同じ見た目（Circle.obj、波紋パルスアニメーション）のClickHintMarkerComponentを配置し、
	// その真上に"Click"というAlphabetTextComponentの子GameObjectを添える。残り3地点は
	// AdvanceClickHintIfClickedがクリックされるたびにこのGameObject自体を移動させて表示する
	// （4組同時に生成せず1組を使い回すことで、順番に1つずつ出現させる演出を実現する）
	clickHintIndex_ = 0;

	// 既存のtag=kClickHintMarker実体（前回Playで生成され、GameObjectとしてscene.jsonに
	// 保存されてしまったもの）が残っていれば削除する。SpawnEnemyAt呼び出し前のtag=Enemy
	// 削除と同じ理由（毎回作り直す方式のため、古い実体が積み重なって複数表示されるのを防ぐ）。
	// ラベル（"Click"テキスト）はmarkerの子のためDeleteObjectsが再帰的に一緒に削除する
	std::vector<GameObject*> existingMarkers;
	for (auto& obj : objects_) {
		if (obj->tag == GameTags::kClickHintMarker) existingMarkers.push_back(obj.get());
	}
	if (!existingMarkers.empty()) DeleteObjects(existingMarkers);

	GameObject& marker = CreateObject("ClickHintMarker");
	marker.tag = GameTags::kClickHintMarker;
	marker.GetTransform().translation = kClickHintMarkerPositions[0];
	auto* markerComp = marker.AddComponent<ClickHintMarkerComponent>();
	markerComp->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	markerComp->pulseMinScale = 0.5f;
	markerComp->pulseMaxScale = 1.5f;
	markerComp->pulseDuration = 3.0f;

	// TitleScene::PLAYボタンと同じ仕組み（OBBColliderComponent+PlayButtonComponent）で
	// このマーカー自身をクリック判定できるようにする。halfSizeはCircle.objの見た目
	// （波紋の最大スケール pulseMaxScale=1.5相当）を覆えるサイズにしておく。他のコライダー
	// （Player/Enemy）と実際に衝突・押し戻しされては困るのでisTrigger=trueにする
	auto* obbCollider = marker.AddComponent<OBBColliderComponent>();
	obbCollider->halfSize = { 1.0f, 1.0f, 1.0f };
	obbCollider->isTrigger = true;
	marker.AddComponent<PlayButtonComponent>(&projectAudioClips_);

	GameObject& label = CreateObject("ClickHintLabel");
	label.GetTransform().translation = { 0.0f, kClickHintLabelYOffset, 0.0f };
	auto* labelText = label.AddComponent<AlphabetTextComponent>();
	labelText->text = "Click";
	labelText->charScale = 0.5f;
	label.SetParent(&marker);

	// CreateObjectで追加したGameObjectをgizmoTargets_（Update/Draw対象一覧）に反映する
	RebuildDerivedLists();
}

void TutorialScene::AdvanceClickHintIfClicked() {
	// TitleScene::PLAYボタンと同じ、マーカー自身のOBBColliderComponent+PlayButtonComponentに
	// よるマウスレイ当たり判定でクリックを検知する。プレイヤーのwaypoint座標を追跡する方式は、
	// Circle.objの見た目サイズに対して座標一致の許容誤差が合わず反応しないバグがあったため撤去した
	GameObject* marker = FindObjectByTag(GameTags::kClickHintMarker);
	if (!marker) return;

	auto* playButton = marker->GetComponent<PlayButtonComponent>();
	if (!playButton || !playButton->ConsumeClicked()) return;

	// 最後の地点（kClickHintMarkerPositionsの末尾＝2度目の右上）が既にクリックされていたら、
	// 循環させずそこで停止する（それ以上は何もしない）
	constexpr size_t kMarkerCount = sizeof(kClickHintMarkerPositions) / sizeof(kClickHintMarkerPositions[0]);
	if (clickHintIndex_ + 1 >= kMarkerCount) return;

	// 次の地点（右上→左上→左下→右下→右上の順）へ、SpawnMoveComponentのイージングで
	// 滑らかに移動する。ラベル（"Click"テキスト）はmarkerの子なので、親のtranslationが
	// 動けば自動的に追従する
	Vector3 startPos = kClickHintMarkerPositions[clickHintIndex_];
	clickHintIndex_++;
	Vector3 targetPos = kClickHintMarkerPositions[clickHintIndex_];

	marker->RemoveComponent<SpawnMoveComponent>();
	auto* spawnMove = marker->AddComponent<SpawnMoveComponent>();
	spawnMove->startPos = startPos;
	spawnMove->targetPos = targetPos;
	spawnMove->duration = kClickHintMoveDuration;
	spawnMove->easing = kClickHintMoveEasing;
	spawnMove->elapsed = 0.0f;
	spawnMove->finished = false;
	spawnMove->animateScale = false;
	spawnMove->destroyOnFinish = false;

	// "Click"の文字ラベルは最初の1回（右上→左上への移動）だけ表示し、以降は隠す
	// （左下・右下への移動時はサークルのみになる）
	if (GameObject* label = marker->GetChildren().empty() ? nullptr : marker->GetChildren().front()) {
		label->GetTransform().translation.y = kClickHintLabelHideY;
	}

	RebuildDerivedLists();
}

void TutorialScene::BeginPreparingPhase() {
	// PlayScene::BeginPreparingPhaseと違い、倒した敵の種類をrespawnQueue_へ積まない
	// （＝補充スポーンしない）。全滅させたら即座に0体のまま計画フェーズへ戻す
	pendingRespawnTags_.clear();
	respawnQueue_.clear();
	if (auto* reflexPlayer = GetReflexPlayer()) {
		reflexPlayer->FinishPreparing();
	}
}

void TutorialScene::ShowHintIfFadeFinished() {
	if (hintShown_) return;
	// FadeManager::State::kIdleは「フェードイン・フェードアウトどちらも行っていない」状態。
	// Title→Tutorial遷移直後はSceneManagerがStartFadeOut()を呼んでいるはずなので、
	// ここでkIdleを見ることは「フェードアウトが完全に消え終わった」ことを意味する
	if (FadeManager::GetInstance().GetState() != FadeManager::State::kIdle) return;
	hintShown_ = true;

	GameObject* hint = FindObjectByTag(GameTags::kTutorialHintAlphabet);
	if (!hint) return;

	// 前回のPlayでStartHintExitAnimation()が完走し、kHintHideYへ退避させた状態のまま
	// シーンが保存されていた場合に備えて、表示するタイミングで必ずY座標を0へ戻しておく
	// （scene.json上の本来の位置。ヒントはtranslation自体を編集する運用を想定していない）
	if (hint->GetTransform().translation.y == kHintHideY) {
		hint->GetTransform().translation.y = 0.0f;
	}

	auto* alphabetText = hint->GetComponent<AlphabetTextComponent>();
	if (!alphabetText) return;
	// textを空→kHintTextへ変えることで、SceneBase::UpdateAlphabetTextComponentsが
	// lastBuiltTextとの不一致を検知してRebuildAlphabetTextChildrenを走らせ、
	// useCharEntranceAnimation=trueなら子文字がここで初めて奥から登場してくる
	alphabetText->text = kHintText;
}

void TutorialScene::StartHintExitAnimation() {
	GameObject* hint = FindObjectByTag(GameTags::kTutorialHintAlphabet);
	if (!hint) return;

	// ClearScene::PlayBackspaceExitAnimationと同じパターン：現在のワールド座標を起点に、
	// Z+方向（奥）へkHintExitZOffsetぶん移動しながらscaleを現在値→0へ縮小する
	Vector3 worldPos = hint->GetWorldTransform().translation;
	hint->RemoveComponent<SpawnMoveComponent>();
	auto* spawnMove = hint->AddComponent<SpawnMoveComponent>();
	spawnMove->startPos = worldPos;
	spawnMove->targetPos = worldPos + Vector3{ 0.0f, 0.0f, kHintExitZOffset };
	spawnMove->duration = kHintExitDuration;
	spawnMove->easing = Easing::Type::kInCubic;
	spawnMove->elapsed = 0.0f;
	spawnMove->finished = false;
	spawnMove->animateScale = true;
	spawnMove->targetScale = hint->GetTransform().scale; // 消える直前の見た目サイズを起点にする
	spawnMove->reverseScale = true; // targetScale→0（縮小しながら消える）
	// destroyOnFinishは名前通りには使わない（GameObjectを実際には削除しない）。ここでは
	// 単に「演出が完了した」合図として使い、HandleSceneTransitionInput側がfinished&&
	// destroyOnFinishを見てkHintHideYへ退避させる（DeleteObjectsしないのはコメント参照）
	spawnMove->destroyOnFinish = true;

	// AddComponent直後はgizmoTargets_（Update対象一覧）に未反映のため、これを呼ばないと
	// SpawnMoveComponent::Updateが回らず退場アニメーションが動かない
	RebuildDerivedLists();
}

bool TutorialScene::AreAllEnemiesDefeated() {
	bool foundAny = false;
	for (auto& obj : objects_) {
		auto* enemy = obj->GetComponent<ReflexEnemyComponent>();
		if (!enemy || enemy->isTemplate || !enemy->enabled) continue;
		foundAny = true;
		break;
	}
	if (foundAny || needsInitialSpawn_) return false;

	// 手動配置のためrespawnQueue_は常に空のはずだが、BeginPreparingPhase以外の経路
	// （例：将来的な補充スポーン追加）でキューが積まれた場合に備えて、消化中は
	// 「まだ出現し切っていないだけ」として全滅判定しないガードを残しておく
	if (!respawnQueue_.empty()) return false;
	if (auto* reflexPlayer = GetReflexPlayer()) {
		if (reflexPlayer->GetPhase() == ReflexPlayerComponent::Phase::kPreparing) return false;
	}
	return true;
}

REGISTER_SCENE(TutorialScene, "Tutorial");
