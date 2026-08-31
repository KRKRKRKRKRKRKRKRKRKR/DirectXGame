#include "ClearScene.h"
#include "GameTags.h"
#include "GameSession.h"
#include "RankingManager.h"
#include "FadeManager.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/Component/Audio/SpawnSoundComponent.h"
#include "../Engine/GameObject/Component/Physics/SpawnMoveComponent.h"
#include "SpawnMovePresets.h"

namespace {
	constexpr const char* kSpawnSoundName = "SpawnSE.mp3";
	// GameObject全体（親）がZ奥から本来の位置へ戻ってくるのにかける時間(秒)
	constexpr float kEntranceDuration = 0.7f;
}

void ClearScene::OnInitialize() {
}

void ClearScene::HideAllUntilEntrance() {
	// Sceneビューで決めた最終位置（scene.json保存済みのtranslation）をtargetPosとして扱い、
	// そこからZ方向にkInitialZOffsetだけ奥へ動かしておく。XY自体は一切変更しない
	// （「シーンビューで場所を決めた場所を最終的な場所とする」という方針のため）。
	// scaleも0にしておくことで、演出開始（ApplySpawnLikeEntrance）までは「何もない空間」に
	// 見えるようにする（Z方向の距離だけでは、近くのカメラなら小さくても見えてしまうため）
	for (const char* tag : { GameTags::kScoreAlphabet, GameTags::kNameInputAlphabet, GameTags::kNextButtonText,
		GameTags::kEnterNamePromptAlphabet, GameTags::kScoreLabelAlphabet, GameTags::kNameInputUnderline }) {
		if (GameObject* obj = FindObjectByTag(tag)) {
			obj->GetTransform().translation.z += kInitialZOffset;
			obj->GetTransform().scale = { 0.0f, 0.0f, 0.0f };

			// SceneBase::UpdateAlphabetTextComponentsはAlphabetTextComponent付きGameObjectの
			// Transform.scaleを毎フレーム無条件にdisplayScaleMultiplier（既定1.0）で上書きする
			// （UpdateGizmoTargets/SpawnMoveComponent::Updateより前に実行される）。そのため上の
			// scale=0だけでは、演出開始（SpawnMoveComponentが付くまで）の待機中に毎フレーム
			// 1.0へ戻されてしまい「最初スケール0になっていない」ように見えていた。
			// displayScaleMultiplier自体も0にしておくことで、待機中の上書き先を0に揃える
			if (auto* alphabetText = obj->GetComponent<AlphabetTextComponent>()) {
				alphabetText->displayScaleMultiplier = 0.0f;
			}
		}
	}
}

void ClearScene::ApplySpawnLikeEntrance(const char* targetTag) {
	GameObject* obj = FindObjectByTag(targetTag);
	if (!obj) return;

	// HideAllUntilEntranceがZ方向にずらした分を差し引き、Sceneビューで決めた本来の位置
	// （targetPos）を求める。obj->GetTransform().translationは現在「奥にずらした状態」
	// （startPos）のままなので、そこからkInitialZOffsetを引き戻したものがtargetPos
	Vector3 startPos = obj->GetTransform().translation;
	Vector3 targetPos = startPos;
	targetPos.z -= kInitialZOffset;

	// PlayScene::BuildEnemyFromTemplateDataのhasSpawnMove分岐と同じ組み立て：奥（startPos）から
	// 本来の位置（targetPos）へkEntranceDuration秒かけてイージングで移動する。ここで即座に
	// targetPosへワープさせず、SpawnMoveComponent::Updateに毎フレーム少しずつ近づけてもらう。
	// animateScale=trueで、同じt・easingを使ってscaleも0→{1,1,1}へ拡大させる（「何もない空間から
	// 出てくる」演出。移動だけだと、近いカメラ位置ではZ方向の距離があっても見えてしまうため）
	auto* spawnMove = obj->AddComponent<SpawnMoveComponent>();
	spawnMove->targetPos = targetPos;
	spawnMove->startPos = startPos;
	spawnMove->duration = kEntranceDuration;
	spawnMove->easing = Easing::Type::kOutCubic;
	spawnMove->elapsed = 0.0f;
	spawnMove->finished = false; // 既定値trueのため明示的にfalseへ戻して演出を開始する
	spawnMove->animateScale = true;
	spawnMove->targetScale = { 1.0f, 1.0f, 1.0f };

	// HideAllUntilEntranceが0にしたdisplayScaleMultiplierを1.0へ戻す。これが0のままだと
	// SceneBase::UpdateAlphabetTextComponentsが毎フレームtransform.scaleを0で上書きし続けて
	// しまい、この直後にSpawnMoveComponentが設定するscaleアニメーションが常に打ち消されてしまう
	// （UpdateAlphabetTextComponentsはUpdateGizmoTargets/SpawnMoveComponent::Updateより前に
	// 実行される設計のため、displayScaleMultiplier側さえ1.0にしておけば、後続の
	// SpawnMoveComponentによるtransform.scale書き換えがそのまま見た目に反映される）
	if (auto* alphabetTextForScale = obj->GetComponent<AlphabetTextComponent>()) {
		alphabetTextForScale->displayScaleMultiplier = 1.0f;
	}

	// AlphabetTextComponentが付いていれば、文字ごとの個別登場演出（左から右への奥行きイージング）
	// を有効にする。NameInputAlphabet（名前入力欄）も対象に含める：SceneBase::
	// RebuildAlphabetTextChildrenの差分検出（skipEntranceIndex）により、1文字入力するたびに
	// 既存の確定済み文字が再演出されることはなく、新しく追加された1文字だけがこの演出の対象になる。
	// ScoreAlphabetはtextProviderもここで初めて設定する（子GameObject＝文字モデルがまだ1つも
	// 存在しない状態から、この関数が呼ばれた瞬間に初めて生成されるようにするため。先に設定すると
	// useCharEntranceAnimationがまだfalseのうちにRebuildAlphabetTextChildrenが走ってしまい、
	// 演出なしで先に見えてしまっていた）
	if (auto* alphabetText = obj->GetComponent<AlphabetTextComponent>()) {
		alphabetText->useCharEntranceAnimation = true;
		if (std::string(targetTag) == GameTags::kScoreAlphabet) {
			alphabetText->SetTextProvider([]() {
				return std::to_string(GameSession::GetInstance().GetScore());
			});
		} else {
			// 案内文・Nextボタン文字列は静的なtext（scene.json保存値）をそのまま使う。
			// lastBuiltTextだけ強制的に不一致にしてRebuildAlphabetTextChildrenを1回発火させる
			alphabetText->lastBuiltText.clear();
		}
	}

	// スポーンSE（SpawnSE.mp3）をPlayシーンの敵テンプレートと共通で鳴らす。projectAudioClips_
	// （Resources/配下を走査した音声ファイル一覧）から表示名で探す（ComponentRegistration.cppの
	// SpawnSoundComponent登録creatorと同じパターン）
	int audioIndex = -1;
	for (size_t i = 0; i < projectAudioClips_.size(); i++) {
		if (projectAudioClips_[i].displayName == kSpawnSoundName) { audioIndex = static_cast<int>(i); break; }
	}
	auto* spawnSound = obj->AddComponent<SpawnSoundComponent>(&projectAudioClips_, audioIndex, 1.0f);
	spawnSound->Play();
}

void ClearScene::PlayBackspaceExitAnimation() {
	GameObject* nameObj = FindObjectByTag(GameTags::kNameInputAlphabet);
	if (!nameObj) return;

	// 削除される文字（末尾の1文字）の子GameObjectを探す。RebuildAlphabetTextChildrenは
	// A〜Z・0〜9以外の文字（カーソル"_"等）には子を生成しないため、"最後に生成された子"が
	// そのまま「今表示されている最後の実文字」になる
	GameObject* lastChar = nullptr;
	for (GameObject* child : nameObj->GetChildren()) {
		if (child->tag == GameTags::kAlphabetChar) lastChar = child;
	}
	if (!lastChar) return; // 表示中の文字が無い（空文字列のまま等）場合は何もしない

	// SetParent(nullptr)は親からの相対座標であるtranslationをそのまま流用してしまう
	// （ワールド座標への変換は行わない）ため、切り離す前に現在のワールド座標を求めて
	// translationへ書き戻す。これで見た目の位置を変えずにルートへ独立させられる
	Vector3 worldPos = lastChar->GetWorldTransform().translation;
	lastChar->SetParent(nullptr);
	lastChar->GetTransform().translation = worldPos;

	// kAlphabetCharのままだと、次のUpdateAlphabetTextComponentsで（既にnameObjの子ではないため
	// 対象外だが）念のためkExitingAlphabetCharへ付け替えて明確に区別する。excludeFromPicking/
	// excludeFromSaveは元々kAlphabetChar生成時に立っている値を維持する
	lastChar->tag = GameTags::kExitingAlphabetChar;

	// 登場演出とは逆に、現在位置（手前）からentranceZOffset奥へ向かってscaleを1→0に
	// 縮小させながら移動する。既存のSpawnMoveComponentが付いていれば（普通は付いていないはずだが
	// 念のため）RemoveしてからAddし直す。パラメータはTutorialScene::StartHintExitAnimationと
	// 共通のSpawnMovePresets::ApplyExitを使う
	lastChar->RemoveComponent<SpawnMoveComponent>();
	auto* spawnMove = lastChar->AddComponent<SpawnMoveComponent>();
	SpawnMovePresets::ApplyExit(*spawnMove, worldPos, lastChar->GetTransform().scale);

	// SetParent(nullptr)でルート直下に移した子を、Gizmo選択対象・Update対象一覧（gizmoTargets_）に
	// 反映する。これを呼ばないとSpawnMoveComponent::Updateが回らず退場アニメーションが動かない
	RebuildDerivedLists();
}

void ClearScene::CleanupExitingChars() {
	std::vector<GameObject*> toDelete;
	for (auto& obj : objects_) {
		if (obj->tag != GameTags::kExitingAlphabetChar) continue;
		auto* spawnMove = obj->GetComponent<SpawnMoveComponent>();
		if (spawnMove && spawnMove->destroyOnFinish && spawnMove->finished) {
			toDelete.push_back(obj.get());
		}
	}
	if (!toDelete.empty()) DeleteObjects(toDelete);
}

void ClearScene::CleanupFinishedSpawnEntrances() {
	// SpawnSoundComponentはPlay()を呼んだ時点（ApplySpawnLikeEntrance内）で役目を終えているため、
	// 毎フレーム無条件に取り除いてよい（scene.jsonへの意図しない保存を防ぐ）。SpawnMoveComponent
	// （GameObject全体のZ移動）はfinished==trueになってから取り除く（Inspectorで演出中に値を
	// 調整したい間は残しておく必要があるため）
	for (const char* tag : { GameTags::kScoreAlphabet, GameTags::kNameInputAlphabet, GameTags::kNextButtonText,
		GameTags::kEnterNamePromptAlphabet, GameTags::kScoreLabelAlphabet, GameTags::kNameInputUnderline }) {
		GameObject* obj = FindObjectByTag(tag);
		if (!obj) continue;
		obj->RemoveComponent<SpawnSoundComponent>();

		if (auto* spawnMove = obj->GetComponent<SpawnMoveComponent>()) {
			if (spawnMove->finished) {
				obj->RemoveComponent<SpawnMoveComponent>();
			}
		}
	}
}

void ClearScene::HandleSceneTransitionInput() {
	// 初回フレームのみ：OnInitialize()の時点ではまだLoadScene()が済んでおらず
	// ScoreAlphabet/NameInputAlphabetが存在しないため、ここ（LoadScene()完了後に必ず呼ばれる
	// 最初のフレーム）でTextProviderを紐付ける。PlayScene::HandleSceneTransitionInputの
	// 同名ブロックと同じ理由
	if (needsInitialBind_) {
		needsInitialBind_ = false;

		// タグ"NameInputAlphabet"のGameObjectにAlphabetTextComponentが付いていれば、入力中の名前を
		// 表示する。末尾に"_"を付けてキーボード直接入力中であることを示す
		if (GameObject* nameObj = FindObjectByTag(GameTags::kNameInputAlphabet)) {
			if (auto* alphabetText = nameObj->GetComponent<AlphabetTextComponent>()) {
				alphabetText->SetTextProvider([this]() {
					return enteredName_ + "_";
				});
			}
		}

		// 画面上の全オブジェクトを、フェードアウト完了後1秒経つまではZ方向の奥に置いておく
		// （敵スポーンで言えば、複製が完了する前から見えてしまっている状態を避けるため）
		HideAllUntilEntrance();
	}

	// 画面全体の登場は、Clear画面へのシーン遷移フェードアウトが完全に終わってから1秒後に
	// 実行する（フェードアウトの途中・直後だと画面がまだ見え始めたばかりで演出の始まりが
	// 分かりづらいため）。FadeManagerの状態がkFadingOut→kIdleへ変わった瞬間を検知し、
	// そこからkPostFadeDelaySeconds秒数えてから、奥に置いていた全オブジェクトを手前へ戻す
	if (spawnEntrancePending_) {
		FadeManager::State fadeState = FadeManager::GetInstance().GetState();
		bool isFadingOut = (fadeState == FadeManager::State::kFadingOut);
		if (wasFadingOut_ && !isFadingOut) {
			// この瞬間フェードアウトが完了した（kIdleに戻った）。以降ここは通らない
			postFadeDelayElapsed_ = 0.0f;
		}
		wasFadingOut_ = isFadingOut;

		if (postFadeDelayElapsed_ >= 0.0f) {
			postFadeDelayElapsed_ += lastDeltaTime_;
			if (postFadeDelayElapsed_ >= kPostFadeDelaySeconds) {
				spawnEntrancePending_ = false;
				ApplySpawnLikeEntrance(GameTags::kScoreAlphabet);
				ApplySpawnLikeEntrance(GameTags::kEnterNamePromptAlphabet);
				ApplySpawnLikeEntrance(GameTags::kNameInputAlphabet);
				ApplySpawnLikeEntrance(GameTags::kNextButtonText);
				ApplySpawnLikeEntrance(GameTags::kScoreLabelAlphabet);
				ApplySpawnLikeEntrance(GameTags::kNameInputUnderline);
			}
		}
	}

	// A〜Zキーで1文字ずつ追加する（ImGuiのテキストボックスに頼らない自作のキーボード直接入力）。
	// DIK_A〜DIK_Zはスキャンコード順（QWERTY配列の物理位置）であり値が連番ではないため、
	// A〜Z順の配列を明示的に並べてループする
	static constexpr BYTE kAlphaKeys[26] = {
		DIK_A, DIK_B, DIK_C, DIK_D, DIK_E, DIK_F, DIK_G, DIK_H, DIK_I, DIK_J,
		DIK_K, DIK_L, DIK_M, DIK_N, DIK_O, DIK_P, DIK_Q, DIK_R, DIK_S, DIK_T,
		DIK_U, DIK_V, DIK_W, DIK_X, DIK_Y, DIK_Z,
	};
	for (int i = 0; i < 26; i++) {
		if (!Input::IsTriggered(kAlphaKeys[i])) continue;
		if (enteredName_.size() >= kMaxNameLength) break;
		enteredName_ += static_cast<char>('A' + i);
		break;
	}
	if (Input::IsTriggered(DIK_BACKSPACE) && !enteredName_.empty()) {
		// enteredName_を書き換える（＝次のUpdateAlphabetTextComponentsで文字が短くなる）前に、
		// 今表示されている最後の文字を退場演出用に差し押さえておく
		PlayBackspaceExitAnimation();
		enteredName_.pop_back();
	}

	// Nextボタンは名前が1文字も無い間、および登場演出が終わってZ奥から手前へ来る前の間は
	// クリックできないようにする（PlayButtonComponent::enabled）。後者が無いと、名前を
	// 先にタイプしておいた場合にZ奥へ配置されたままのボタンへ判定上はクリックが通ってしまう
	// （カメラからのレイはXYだけでなくZ方向の距離自体はヒット判定を妨げないため）。
	// 見た目（グレー表示）もこのenabledに連動させ、押せない状態であることを視覚的に示す
	bool canProceed = !enteredName_.empty() && !spawnEntrancePending_;
	GameObject* nextHitbox = FindObjectByTag(GameTags::kNextButtonHitbox);
	auto* nextButton = nextHitbox ? nextHitbox->GetComponent<PlayButtonComponent>() : nullptr;
	if (nextButton) {
		nextButton->enabled = canProceed;
	}

	// spawnEntrancePending_の間（登場演出の待機中〜アニメーション中）は、見た目を毎フレーム
	// 上書きしない。ApplySpawnLikeEntranceがdisplayScaleMultiplierを0→1に制御しつつ
	// SpawnMoveComponentでscaleアニメーションさせている最中に、ここでnormalScaleMultiplier
	// （既定1.0）へ毎フレーム戻されてしまうと、NextButtonTextだけ「登場演出の0スケールが
	// 一切効かない」状態になっていた（他のオブジェクトはこの種の毎フレーム上書きロジックを
	// 持たないため影響を受けていなかった）。
	// canProceed==trueの間はSceneBase::UpdateButtonAndReflectHoverでホバー反映＋クリック検知を
	// 行う。false（disabledColorでのグレー表示）の間は共通関数の2値（hover/normal）では
	// 表現できないため、ここだけ個別に処理する
	bool nextClicked = false;
	if (!spawnEntrancePending_) {
		if (canProceed) {
			auto nextResult = UpdateButtonAndReflectHover(GameTags::kNextButtonHitbox, GameTags::kNextButtonText);
			nextClicked = nextResult.clicked;
		} else if (nextButton) {
			// 押せない間は常にグレー表示・等倍のままにする（ホバー演出も出さない）
			if (GameObject* nextButtonTextObj = FindObjectByTag(GameTags::kNextButtonText)) {
				if (auto* text = nextButtonTextObj->GetComponent<AlphabetTextComponent>()) {
					text->displayScaleMultiplier = nextButton->normalScaleMultiplier;
					text->displayColor = nextButton->disabledColor;
				}
			}
		}
	}

	if (nextClicked) {
		// canProceed==falseの間はそもそもUpdateButtonAndReflectHoverを呼ばないためnextClickedは
		// 常にfalseになる（上のif (canProceed)分岐参照）。念のためもう一度空チェックしておく
		if (!enteredName_.empty()) {
			GameSession::GetInstance().SetEnteredName(enteredName_);
			size_t submittedIndex = RankingManager::GetInstance().Submit(enteredName_, GameSession::GetInstance().GetScore());
			// RankingSceneが遷移直後、自分の順位まで自動スクロール＋ハイライトするために使う
			GameSession::GetInstance().SetLastSubmittedEntryIndex(submittedIndex);
			nextScene_ = "Ranking";
		}
	}

	// 登場演出が完了した一時コンポーネントを片付ける（scene.jsonへの意図しない保存を防ぐ）
	CleanupFinishedSpawnEntrances();

	// Backspaceの退場アニメーションが完了した文字GameObjectを片付ける
	CleanupExitingChars();
}

REGISTER_SCENE(ClearScene, "Clear");
