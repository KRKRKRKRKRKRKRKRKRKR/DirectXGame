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

	// SceneBase::RebuildAlphabetTextChildrenが生成する、AlphabetTextComponent1文字分の
	// 子GameObjectの目印。ユーザーが手動でこのタグを付けたGameObjectがあると
	// ClearAlphabetTextChildrenの一括削除に巻き込まれるため、他の用途では使わないこと
	inline constexpr const char* kAlphabetChar = "AlphabetChar";

	// SceneBase::RebuildDashedLineSegmentsが生成する、DashedLineComponent1本分の
	// 子GameObjectの目印。ユーザーが手動でこのタグを付けたGameObjectがあると
	// ClearDashedLineSegmentsの一括削除に巻き込まれるため、他の用途では使わないこと
	inline constexpr const char* kDashedLineSegment = "DashedLineSegment";

	// PlayScene::OnInitializeが起動時に探す、撃破数をAlphabetTextComponentの3Dモデル文字で
	// 表示するGameObjectの目印。ユーザーがInspectorでAlphabetTextComponentを追加したGameObjectに
	// このタグを付けると、PlayScene側がkillCount_を文字列化するTextProviderを自動で紐付ける
	inline constexpr const char* kKillCountAlphabet = "KillCountAlphabet";

	// PlayScene::OnInitializeが起動時に探す、残りラウンド数（目標ラウンド数からのカウントダウン）を
	// AlphabetTextComponentの3Dモデル文字で表示するGameObjectの目印。kKillCountAlphabetと同じ運用
	inline constexpr const char* kRoundCountAlphabet = "RoundCountAlphabet";

	// PlayScene::OnInitializeが起動時に探す、現在のスコア（score_）をAlphabetTextComponentの
	// 3Dモデル文字で表示するGameObjectの目印。kKillCountAlphabetと同じ運用
	inline constexpr const char* kScoreAlphabet = "ScoreAlphabet";

	// SceneBase::SpawnComboPopupが生成する、コンボポップアップ1個分の親GameObject（数字の桁を
	// まとめる箱）の目印。ComboPopupComponent::ActivePopup::modelObjectが指すGameObjectに付く
	inline constexpr const char* kComboPopup = "ComboPopup";

	// TitleScene::HandleSceneTransitionInputが探す、PLAYボタンの見た目（AlphabetTextComponent、
	// text="PLAY"）を持つGameObjectの目印。PlayButtonComponent（当たり判定用GameObjectに付く）が
	// ホバー中に色・サイズを変化させる対象として、このタグでFindObjectByTagして自動的に紐付ける
	inline constexpr const char* kPlayButtonText = "PlayButtonText";

	// TitleScene::HandleSceneTransitionInputが探す、PLAYボタンの当たり判定（OBBColliderComponent+
	// PlayButtonComponent）を持つGameObjectの目印
	inline constexpr const char* kPlayButtonHitbox = "PlayButtonHitbox";

	// ClearScene::HandleSceneTransitionInputが起動時に探す、入力中の名前をAlphabetTextComponentの
	// 3Dモデル文字で表示するGameObjectの目印。kKillCountAlphabetと同じ運用
	inline constexpr const char* kNameInputAlphabet = "NameInputAlphabet";

	// ClearScene::HandleSceneTransitionInputが探す、Nextボタンの見た目（AlphabetTextComponent、
	// text="NEXT"）を持つGameObjectの目印。kPlayButtonTextと同じ役割
	inline constexpr const char* kNextButtonText = "NextButtonText";

	// ClearScene::HandleSceneTransitionInputが探す、Nextボタンの当たり判定（OBBColliderComponent+
	// PlayButtonComponent）を持つGameObjectの目印。kPlayButtonHitboxと同じ役割。名前が未入力の間は
	// このGameObjectのPlayButtonComponent::enabledをfalseにしてクリックを無効化する
	inline constexpr const char* kNextButtonHitbox = "NextButtonHitbox";

	// ClearScene::HandleSceneTransitionInputが起動時に探す、「please enter your name」等の
	// 案内文言を表示するGameObjectの目印。kScoreAlphabetと合わせて、Clear画面に入った瞬間
	// PlayScene::SpawnEnemyAtの敵スポーン演出（ランダム位置＋Z方向からのイージング移動＋
	// スポーンSE）を模した登場演出を1回だけ適用する対象になる
	inline constexpr const char* kEnterNamePromptAlphabet = "EnterNamePromptAlphabet";

	// ClearScene::HandleSceneTransitionInputが起動時に探す、「SCORE」というラベル文字
	// （数値そのものではなく見出し）を表示するGameObjectの目印。kScoreAlphabet等と同じ
	// 登場演出（Z方向からのイージング移動＋スケール0→1）を適用する対象になる
	inline constexpr const char* kScoreLabelAlphabet = "ScoreLabelAlphabet";

	// ClearScene::HandleSceneTransitionInputが起動時に探す、名前入力欄の下の破線
	// （DashedLineComponent）を持つGameObjectの目印。kScoreAlphabet等と同じ登場演出
	// （Z方向からのイージング移動＋スケール0→1）を適用する対象になる
	inline constexpr const char* kNameInputUnderline = "NameInputUnderline";

	// ClearScene::PlayBackspaceExitAnimationが、Backspaceで削除される直前の文字GameObjectに
	// 付け替えるタグ。kAlphabetCharから外すことでSceneBase::ClearAlphabetTextChildren
	// （次の再構築での一括削除）の対象から除外しつつ、CleanupExitingCharsが退場アニメーション
	// 完了後にこのGameObjectを見つけて削除できるようにする目印
	inline constexpr const char* kExitingAlphabetChar = "ExitingAlphabetChar";

	// RankingScene::RebuildRankingRowsが動的生成する、ランキング1行分の列（AlphabetTextComponent）の
	// GameObjectの目印。RankingComponentが付いたGameObjectの子（行）のさらに子として生成され、
	// エントリ件数が変わった際に一括削除する対象として使う。ページ送り方式を廃止したため、
	// 一度生成した行はスクロール中も作り直さない（translation.yの再計算だけで反映する）
	inline constexpr const char* kRankingRow = "RankingRow";

	// TitleScene::HandleSceneTransitionInputが探す、ランキング画面へ遷移するボタンの見た目
	// （AlphabetTextComponent、text="RANKING"）を持つGameObjectの目印。kPlayButtonTextと同じ役割
	inline constexpr const char* kViewRankingButtonText = "ViewRankingButtonText";

	// TitleScene::HandleSceneTransitionInputが探す、ランキング画面へ遷移するボタンの当たり判定
	inline constexpr const char* kViewRankingHitbox = "ViewRankingHitbox";

	// RankingScene::HandleSceneTransitionInputが探す、Titleへ戻るボタンの見た目
	// （AlphabetTextComponent、text="TITLE"等）を持つGameObjectの目印。kNextButtonTextと同じ役割
	inline constexpr const char* kRankingTitleButtonText = "RankingTitleButtonText";

	// RankingScene::HandleSceneTransitionInputが探す、Titleへ戻るボタンの当たり判定
	// （OBBColliderComponent+PlayButtonComponent）を持つGameObjectの目印。kNextButtonHitboxと同じ役割
	inline constexpr const char* kRankingTitleButtonHitbox = "RankingTitleButtonHitbox";

	// TutorialScene::HandleSceneTransitionInputが探す、「CLICK to move」等の操作説明文言を
	// 表示するGameObjectの目印。プレイヤーが最初のクリック（経路予約の1点目）をした瞬間に
	// このタグのGameObjectを非表示にする
	inline constexpr const char* kTutorialHintAlphabet = "TutorialHintAlphabet";

	// TutorialScene::AdvanceClickHintIfClickedが探す、現在表示中の「クリックを促すマーカー」
	// （ClickHintMarkerComponent、Circle.obj＋波紋パルス）を持つGameObjectの目印。
	// kClickHintMarkerPositionsを1つずつ順番に移動しながら表示するため、シーン内には常に
	// このタグのGameObjectが1つだけ存在する
	inline constexpr const char* kClickHintMarker = "ClickHintMarker";
}
