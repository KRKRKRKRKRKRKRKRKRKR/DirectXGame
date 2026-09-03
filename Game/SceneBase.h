#pragma once
#include "IScene.h"
#include "../Engine/Graphics/Renderer/Renderer.h"
#include "../Engine/Graphics/Pipeline/BlendMode.h"
#include "../Engine/Camera/Camera.h"
#include "../Math/MathTypes.h"
#include "../Math/Collision.h"
#include "../Externals/imgui/imgui.h" // ImGuizmo.hより先にインクルードする必要がある
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Engine/GameObject/GameObject.h"
#include "../Engine/GameObject/Systems/ColliderSystem.h"
#include "../Engine/GameObject/Systems/GizmoController.h"
#include "../Engine/GameObject/Component/Render/Render.h"
#include "../Engine/GameObject/Component/Physics/Physics.h"
#include "../Engine/GameObject/Component/Lighting/Lighting.h"
#include "../Engine/GameObject/Component/Audio/Audio.h"
#include "../Engine/GameObject/Component/Camera/Camera.h"
#include "../Engine/GameObject/ComponentLoadContext.h"
#include "SceneObjectStore.h"
#include "SceneRegistry.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <utility>
#include <unordered_map>

// GameObjectエディタ機能一式（生成・削除・Gizmo編集・当たり判定・UI/Object分割Save/Load・
// HUD/説明文テキスト作成・ライティング）を持つIScene実装の共通基底。PlayScene/TitleScene等は
// これを継承し、シーンごとに異なる部分（起動直後に何を生成するか、シーン遷移キー入力）だけを
// OnInitialize()/HandleSceneTransitionInput()でオーバーライドする
class SceneBase : public IScene {
public:
	void Initialize(Renderer* renderer, Camera* camera, const std::string& assetFolder) override;
	void Render(float deltaTime) override;

	std::string GetNextScene() const override { return nextScene_; }
	void RequestSave() override { SaveScene(); }

protected:
	// SceneBase::Initializeの最後（RebuildDerivedLists()の直前）に呼ばれる。
	// 起動直後に生成しておきたいシーン固有のGameObject（HUD等）があれば派生クラスでオーバーライドする
	virtual void OnInitialize() {}

	// SceneBase::Renderの最後に呼ばれる。シーン固有のキー入力による遷移条件を
	// nextScene_への代入で表現する（デフォルトは何もしない＝遷移しない）
	virtual void HandleSceneTransitionInput() {}

	// SceneBase::DrawInspector()が selected->DrawImGui() の直後、DrawAddComponentMenu()の前に呼ぶ
	// 拡張フック。SceneBase.cppが知るべきでない派生シーン固有の具体型（例：PlayScene固有の
	// ReflexEnemySpawnerComponent専用UI）を追加したい派生シーンはこれをオーバーライドする。
	// デフォルトは何もしない（TitleScene/GameOverScene等はオーバーライド不要）
	virtual void DrawSceneSpecificInspectorExtensions(GameObject& selected) {}

	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	std::string nextScene_;

	// 「常に警告」方式の保存確認：実際に変更があったかは追跡せず、シーン遷移が要求される
	// （nextScene_がセットされる）たびに毎回確認モーダルを挟む。HandleSceneTransitionInputと
	// Objects/Gizmoパネルの手動切替ボタンのどちらも最終的にnextScene_へ代入するだけなので、
	// Render()の末尾でnextScene_を一旦ここに退避し、確認が済むまでSceneManagerに見せない
	std::string pendingTransitionRequest_;
	bool showTransitionSavePrompt_ = false;
	void DrawTransitionSavePrompt();

	// SceneManagerから渡される素材フォルダ名（例:"Play"/"Title"）。SaveScene/LoadScene内で
	// Resources/{assetFolder_}/ui.json・Resources/{assetFolder_}/scene.jsonの組み立てに使う
	std::string assetFolder_;

	// Unityの Play/Stop 相当。false(Stop)ではGameObject::Update()と押し戻しを止め、Gizmoで自由に配置できる
	bool isPlaying_ = false;

	// シーン内の全GameObject。unique_ptrで持つのは、vector再配置後もGameObject自体のアドレスが
	// 変わらないようにするため（gizmoTargets_/screenTargets_が生ポインタで参照し続ける）
	std::vector<std::unique_ptr<GameObject>> objects_;

	// 新しいGameObjectを生成してobjects_へ追加し、安定した参照を返す
	GameObject& CreateObject(const std::string& name);

	// tagが一致する最初のGameObjectを返す（無ければnullptr）。Unityの
	// GameObject.FindWithTag相当。「シーン内で最初に見つかったXを機械的に使う」だけでは
	// 複数存在する場合に選べなかった箇所（メインカメラ・プレイヤー等）で使う
	GameObject* FindObjectByTag(const std::string& tag);

	// 「タグでhitbox取得→PlayButtonComponent取得→IsHovering()をAlphabetTextComponentの
	// displayColor/displayScaleMultiplierへ反映→ConsumeClicked()の結果を返す」という、
	// TitleScene(PLAY/Rankingボタン)・ClearScene(Nextボタン)・RankingScene(Titleボタン)で
	// 個別に複製されていた同一骨格をまとめたヘルパー。呼び出し側は戻り値のclickedを見て
	// シーン遷移すればよい。hitboxTagのGameObjectが無い/PlayButtonComponentが無い場合は
	// 何もせずclicked=false/hovering=falseを返す。textTagが無い（nullptrや未配置）場合は
	// 見た目の反映だけスキップする（ClickHintMarkerのように見た目を持たないボタンにも使えるが、
	// 現状はTutorialScene::AdvanceClickHintIfClickedが既存のまま個別実装している）
	struct ButtonInteractionResult {
		bool clicked = false;
		bool hovering = false;
	};
	ButtonInteractionResult UpdateButtonAndReflectHover(const char* hitboxTag, const char* textTag);

	// Gizmoパネルで選択中のオブジェクトをobjects_から削除する
	void DeleteSelectedObject();

	// rootsに含まれる各オブジェクトとその子孫すべてをobjects_から削除する（カスケード削除）。
	// DeleteSelectedObject（選択中を消す）とHierarchyの右クリックメニュー「削除」
	// （右クリックした特定のノードを消す）の両方から共通で使う
	void DeleteObjects(const std::vector<GameObject*>& roots);

	// "Objects"パネルの"Add Component"セクション。selectedに対して、ComponentRegistryの
	// Simple系（引数なしで安全に追加できる型）はコンボ+Addボタンで一覧から、依存のある型
	// （Model/Sprite/TextureSelector/Mirror/AudioSource）は個別の入力欄+ボタンで追加する。
	// Cube等の完成形をArchetypeから1発生成する既存フローとは別に、空のGameObjectへ
	// 後から機能を積み上げていく生成スタイルを提供する
	void DrawAddComponentMenu(GameObject& selected);

	// DrawAddComponentMenuの「描画」「オーディオ」カテゴリ配下、8種のTreeNodeブロックを
	// それぞれ抽出したもの。各関数は自分のTreeNode内のstatic入力バッファを保持し、
	// ComponentRegistry::Createまでを完結させる。ctxはDrawAddComponentMenuがMakeComponentLoadContext()
	// で1回だけ作った値を全ブロックで使い回す（既存の挙動を維持するため引数で渡す）
	void DrawAddModelRenderNode(GameObject& selected, const ComponentLoadContext& ctx, bool alreadyHasRenderComponent);
	void DrawAddSpriteRenderNode(GameObject& selected, const ComponentLoadContext& ctx, bool alreadyHasRenderComponent);
	void DrawAddTextSpriteNode(GameObject& selected, const ComponentLoadContext& ctx, bool alreadyHasRenderComponent);
	void DrawAddTextureSelectorNode(GameObject& selected, const ComponentLoadContext& ctx);
	void DrawAddMirrorNode(GameObject& selected, const ComponentLoadContext& ctx);
	void DrawAddReflexEnemyHealthBarNode(GameObject& selected, const ComponentLoadContext& ctx);
	void DrawAddAudioSourceNode(GameObject& selected, const ComponentLoadContext& ctx);
	void DrawAddHitSoundNode(GameObject& selected, const ComponentLoadContext& ctx);
	void DrawAddSpawnSoundNode(GameObject& selected, const ComponentLoadContext& ctx);
	void DrawAddPlayButtonNode(GameObject& selected, const ComponentLoadContext& ctx);

	// objects_の保存/復元自体はSceneObjectStoreに委譲する（ファイルパス組み立て・
	// is2D振り分け・SceneSerializer呼び出しはそちらの責務）。ここではLoad後に必要な
	// Rebind/RebuildDerivedLists/ResetSelectionの後始末だけを行う。
	// saveNameを指定すると、既定のscene.json/ui.jsonとは別の名前付きスナップショットを
	// 保存/読み込みする（省略時は今まで通りの既定ファイル）
	void SaveScene(const std::string& saveName = "");
	void LoadScene(const std::string& saveName = "");

	// Resources/{assetFolder_}/を走査し、scene_*.jsonから名前付きスナップショット一覧を
	// 作り直す。Initialize()で1回、名前を付けて保存した直後にも呼ぶ
	void RescanSavedSnapshots();
	std::vector<std::string> savedSnapshotNames_;
	int selectedSnapshotIndex_ = 0;

	// Render()の最後に更新する直近のdeltaTime。PlayScene::UpdatePreparingPhase等、
	// HandleSceneTransitionInput（引数を持たない）側から「今フレームのdeltaTime」を
	// 参照したい派生シーンのために保持する
	float lastDeltaTime_ = 0.0f;

	// excludeFromGizmoListを見てgizmoTargets_をobjects_から作り直す。
	// オブジェクトの生成・削除・ロードの後に必ず呼ぶ
	void RebuildDerivedLists();

	std::vector<TextureEntry> textures_;

	// Gizmo選択対象一覧（excludeFromGizmoList=falseのオブジェクトのみ）
	std::vector<GameObject*> gizmoTargets_;

	// スクリーン空間UI（TransformComponent::is2D==true）のマウス選択・Gizmo操作対象一覧。
	// gizmoTargets_（3D、ワールド空間レイキャスト）とは別に、GizmoControllerの2D版
	// （px座標のAABBピッキング＋正射影ギズモ）が扱う
	std::vector<GameObject*> screenTargets_;

	ColliderSystem colliderSystem_;
	GizmoController gizmoController_;

	void DrawGrid();
	void DrawImGui();

	// DrawImGui()（ギズモウィンドウ）のセクション分割。呼び出し順序はウィンドウ内の表示順序と
	// 一致させる（挙動・条件分岐は元のDrawImGuiと同一、コメント区切り単位で抽出しただけ）
	void DrawPlayStopControls();       // Play/Stopボタンと状態テキスト
	void DrawSceneGameViewToggle();    // Scene/Gameビュー切替ボタン（カメラの有無で活性/非活性を制御）
	void DrawSceneSaveLoadControls();  // 保存/読み込みボタン、「名前を付けて保存」ポップアップ、保存済みスナップショットのコンボ
	void DrawSceneTransitionButtons(); // SceneRegistryへ登録済みの全シーン名を列挙したシーン切替ボタン群

	void DrawHierarchy();

	// DrawHierarchyの木構造描画（旧drawNode/drawInsertionGapラムダ）を切り出したヘルパー。
	// 1回のDrawHierarchy呼び出しの間だけ生存する一時状態（選択中オブジェクト、右クリック削除の
	// 保留先）を持つため、SceneBaseのメンバ変数ではなくこのネストクラスのメンバとして
	// スコープを最小限に保つ。定義はSceneBase.cpp（DrawHierarchy専用、外部から使わない）
	class HierarchyTreeDrawer;

	void DrawInspector();

	// Unityの「Projectビュー」相当。ユーザーが追加した画像・音声・モデルをアイコンの一覧として
	// 表示し、ドラッグ&ドロップでオブジェクトへ付与できるようにする
	void DrawProjectPanel();

	// DrawProjectPanelの画像/音声/モデルグリッド共通処理。PushID→BeginGroup→アイコン描画
	// （drawIconに委譲）→ラベル→EndGroup→BeginDragDropSource→列送り改行、という3カテゴリで
	// 完全に同一の手順をここへ集約する。kIconSize/kTileWidth/columnCountは呼び出し元が
	// 1回だけ計算した値を渡す（3回計算し直さないため）
	void DrawProjectAssetGrid(
		const std::vector<ProjectAssetEntry>& assets, const char* dragDropId,
		float iconSize, float tileWidth, int columnCount,
		const std::function<void(const ProjectAssetEntry&, float iconSize)>& drawIcon);

	// Resources/配下を走査してprojectImages_/projectAudioClips_/projectModels_を作り直す。
	// Initialize()で1回、以降はプロジェクトパネルの「更新」ボタンから呼ばれる
	void RescanProjectAssets();
	std::vector<ProjectAssetEntry> projectImages_;
	std::vector<ProjectAssetEntry> projectAudioClips_;
	std::vector<ProjectAssetEntry> projectModels_;

	// pathの画像がtextures_（TextureSelectorComponentが参照する共有テクスチャ一覧）に
	// 無ければLoadTexture+登録し、表示名（textures_内でのname）を返す
	std::string EnsureTextureRegistered(const std::string& path);

	// ComponentLoadContext{renderer_, &textures_, ensureTextureRegisteredコールバック}の組み立てを
	// 1箇所にまとめる（呼び出し箇所が複数あり、フィールドが増えるたびに全箇所を書き換えずに済むように）
	ComponentLoadContext MakeComponentLoadContext();

	// プロジェクトパネルからのドロップ受け入れ処理。画像はRenderComponentBaseを持つ相手にのみ
	// TextureSelectorComponentを付与/差し替え、音声はAudioSourceComponentを付与する
	void AttachTextureAsset(GameObject& obj, const std::string& path);
	void AttachAudioAsset(GameObject& obj, const std::string& path);

	// モデルはそれ自体がRenderComponentBase（ModelRenderComponent）なので、画像/音声と違い
	// 「RenderComponentBaseを持たないGameObjectにのみ」新規追加する（既に何か描画コンポーネントが
	// 付いている相手には、Sprite/Model重複防止ガードと同じ理由で付与しない）
	void AttachModelAsset(GameObject& obj, const std::string& path);

	// プロジェクトパネルの「+ 新規スクリプト」ボタンの実処理。baseNameから
	// className="{baseName}Component"（既にComponent終わりなら付け足さない）、
	// typeName=baseNameを決め、Game/Scripts/へひな形.h/.cppを生成し、.vcxproj/.vcxproj.filters
	// へ自動登録したうえでエディタ（既定の関連付けアプリ）で開く。結果メッセージは
	// lastScriptCreationMessage_へ入れ、DrawProjectPanel()が数フレーム表示する
	void CreateNewScript(const std::string& baseName, const std::string& displayName, const std::string& category);
	std::string lastScriptCreationMessage_;

	// Hierarchyのドラッグ&ドロップ並べ替え用。droppedを親なし（ルート）にした上で、
	// ルート表示対象（親なし・excludeFromGizmoList=falseのオブジェクト）の中でvisibleIndex番目
	// （挿入後の位置、0=先頭）に来るよう、objects_内での実際の位置を調整する
	void ReorderRootObject(GameObject* dropped, size_t visibleIndex);

	// Unityの Scene/Game タブ相当。falseはエディタ自由カメラ+Gizmo（Scene）、
	// trueはシーン内カメラ視点でGizmoなし（Game）。ボタンでの切替はDrawImGui()内で行う
	bool viewingGameCamera_ = false;

	// ==== Render()の分割ヘルパー（処理順序・条件分岐はRender本体と完全に同一。
	// 可読性のためコメント区切り単位で抽出しただけで、挙動は一切変えていない） ====

	// Gameモード用カメラの解決結果（見つからなければgameCamera==nullptr）
	struct GameCameraResolution {
		CameraComponent* gameCamera = nullptr;
		GameObject* gameCameraObject = nullptr;
	};

	// そのフレームで実際に使うview/proj/カメラ位置と、Game/Sceneどちらを見ているかのフラグ。
	// シェイク適用後の値（Mirrorパスが必要とする「生」のview/projとは別物）
	struct ActiveCameraState {
		Matrix4x4 view;
		Matrix4x4 proj;
		Vector3   camPos;
		bool      useGameCamera = false;
	};

	// Mirrorコンポーネントの解決結果（見つからなければmirror==nullptr）
	struct MirrorResolution {
		MirrorComponent* mirror = nullptr;
		GameObject* mirrorObject = nullptr;
	};

	// Render()がResolveActiveCamera()で毎フレーム計算した最新値を控えておいたもの。
	// HandleSceneTransitionInput（ProcessSceneTransitionRequest経由でRender()の後半から呼ばれる）は
	// 引数を持たない仮想関数のため、3Dレイキャストが必要な派生シーン（RankingScene::
	// UpdateScrollBarのつまみドラッグ判定等）がScreenRay::FromMouse用のview/projを得る手段として使う
	ActiveCameraState lastActiveCameraState_;

	// CameraFollowComponentのtarget解決（タグ"Player"優先、無ければAutoRunComponent持ちにフォールバック）
	void UpdateAutoRunCameraFollowTarget();

	// AlphabetTextComponentを持つ全GameObjectについて、text（今回表示したい文字列）が
	// lastBuiltText（前回子GameObjectを組み立てた時点の文字列）と食い違っていたら、
	// 既存の文字の子GameObjectを全部削除してtextに応じて作り直す。Render()から毎フレーム呼ぶ
	// （詳しくはAlphabetTextComponent.h参照）
	void UpdateAlphabetTextComponents();

	// ownerの子のうちtag==kAlphabetCharTagのものを全部削除する（RebuildAlphabetTextChildrenが
	// 作り直す前の後始末、およびLoadScene直後の「保存されてしまった古い子」の掃除に使う）
	void ClearAlphabetTextChildren(GameObject& owner);

	// ownerの下にcomp.textの内容に応じた文字の子GameObjectを新規生成する。1文字ごとに
	// Resources/Alphabet/{文字}.objをModelRenderComponentとして読み込み、charSpacing間隔で
	// X軸方向に等間隔に並べる（全体の横幅の中心がownerのtranslationに来るよう左右対称に配置する）。
	// 対応する.objが無い文字（A〜Z・0〜9・半角スペース以外）は、その1文字分の間隔だけ空けて
	// 何も生成しない
	void RebuildAlphabetTextChildren(GameObject& owner, AlphabetTextComponent& comp);

	// 'A'〜'Z'（大文字化して引数に渡す）・'0'〜'9'のRenderer::ModelHandleをキャッシュする。
	// 同じ文字が文字列内で繰り返し使われても、Renderer::LoadModelを呼び直さず使い回す
	// （LoadModelは呼ぶたびに新しいModelHandle/SRVスロットを消費するため）
	Renderer::ModelHandle GetOrLoadAlphabetModel(char upperLetter);
	std::unordered_map<char, Renderer::ModelHandle> alphabetModelCache_;

	// DashedLineComponentを持つ全GameObjectについて、dashCount/dashWidth/dashThickness/
	// dashSpacingが前回組み立てた時点の値と食い違っていたら、既存のダッシュの子GameObjectを
	// 全部削除して作り直す。Render()から毎フレーム呼ぶ（UpdateAlphabetTextComponentsと同じパターン）
	void UpdateDashedLineComponents();

	// ownerの子のうちtag==kDashedLineSegmentのものを全部削除する
	void ClearDashedLineSegments(GameObject& owner);

	// ownerの下にcomp.dashCount本ぶんのダッシュ（薄いCube）を、中心がownerのtranslationに
	// 来るよう左右対称にdashSpacing間隔で並べる
	void RebuildDashedLineSegments(GameObject& owner, DashedLineComponent& comp);

	// ComboPopupComponentを持つ全GameObject（通常はPlayer）について、
	// 1) ConsumePendingRequest()で新しいコンボ値のリクエストがあれば、表示中のポップアップを
	//    即座に破棄して新しい値でポップインをやり直す（キルカウントHUDと同じ「1つの表示が
	//    値の更新に合わせて差し替わる」方式）、
	// 2) ConsumeClearRequested()が立っていれば表示中のポップアップを即座に破棄、
	// 3) 表示中のポップアップのelapsedを進め、ポップイン/静止/フェードアウトの現在フェーズに
	//    応じてscale・alphaをイージングで更新する。静止表示中にholdDurationを超えたら
	//    自動的にフェードアウトへ移行し、フェードアウトが終わったら破棄する。
	// Render()から毎フレーム呼ぶ（UpdateAlphabetTextComponentsと同様の「シーン側が実体を管理する」パターン）
	void UpdateComboPopupComponents(float deltaTime);

	// comboValue（1個の整数値、複数桁ありうる）を表示する1個のポップアップを新規生成する。
	// ownerの下に「1個のポップアップ全体を表す」空の親GameObject（グループ）を作り、その下に
	// 数字を1桁ずつAlphabetTextComponentと同じ要領でModelRenderComponent付き子として並べる。
	// グループGameObject自身をComboPopupComponent::ActivePopup::modelObjectとして登録することで、
	// UpdateComboPopupComponentsが桁ごとの子を意識せず、グループ単位でscale/alpha・破棄を扱える
	void SpawnComboPopup(GameObject& owner, ComboPopupComponent& comp, int comboValue);

	// タグ"MainCamera"優先、無ければシーン内最初のCameraComponentにフォールバックしてGameカメラを探す
	GameCameraResolution ResolveGameCamera();

	// view/proj（Sceneフリーカメラの生値）とgameCamの解決結果から、そのフレームで実際に使う
	// view/proj/camPos/useGameCameraを確定させる。isPlaying_中はカメラシェイクもここで適用する
	ActiveCameraState ResolveActiveCamera(const Matrix4x4& view, const Matrix4x4& proj,
		const GameCameraResolution& gameCam, float deltaTime);

	// 敵ヒット時のヒットストップ中はgameplayDeltaTimeを0にする（実時間でのタイマー消化とは別）
	float ComputeGameplayDeltaTime(float deltaTime) const;

	// isPlaying_中のみ、gizmoTargets_の各GameObjectへUpdateを配る
	void UpdateGizmoTargets(float gameplayDeltaTime, const ActiveCameraState& activeCam);

	// Scene表示中のみ、Gizmoの操作（ドラッグ編集）・右クリックメニューを更新する。
	// クリックによる選択変更はUpdateGizmoPickingLateClickに分離されている（そちらのコメント参照）
	void UpdateGizmoPicking(const ActiveCameraState& activeCam);

	// Scene表示中のみ、DrawImGui()の後でクリックによる選択変更を判定する
	// （UpdateGizmoPickingLateClickの実装コメント参照）
	void UpdateGizmoPickingLateClick(const ActiveCameraState& activeCam);

	// シーン内のMirrorComponentを探す（複数あっても最初の1つのみ対応）
	MirrorResolution FindMirror();

	// 鏡の反射視点でオフスクリーンへ描画する（Mirror自身とSprite2Dは映さない）。
	// view/projは反射計算に使う生のカメラ行列、activeCamはパス終了後にSetCameraへ戻す値
	void RenderMirrorPass(const MirrorResolution& mirror, const Matrix4x4& view, const Matrix4x4& proj,
		const ActiveCameraState& activeCam);

	// 通常の描画ループ（Mirror自身は反射テクスチャ確定後に別途描画するためスキップする）
	void RenderMainPass(float deltaTime);

	// Mirror自身の描画（反射テクスチャが確定した後に描く）
	void DrawMirrorObject(const MirrorResolution& mirror);

	// 各ライトコンポーネントのSyncToRenderer呼び出し（デバッグ可視化はScene表示中のみ）
	void SyncLighting(const ActiveCameraState& activeCam);

	// CameraComponentのワイヤーフレーム可視化（Scene表示中のみ）
	void DrawCameraGizmoVisualizations(const ActiveCameraState& activeCam);

	// エディタUI表示中のみDrawImGui()を呼ぶ
	void DrawEditorUiIfVisible();

	// シーン遷移要求（nextScene_）を検知し、エディタUI表示中なら保存確認を挟んでから
	// pendingTransitionRequest_へ退避する
	void ProcessSceneTransitionRequest();
};
