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

	// GameObject生成・スクリーン空間設定・TextRenderComponent::CreateDynamic呼び出し・
	// SetTextProviderをまとめて行う（Camera座標HUD等、呼び出し側の引数を
	// 「何を・どのフォントで・何を表示するか」だけに絞るためのラッパー）
	GameObject& CreateDynamicTextObject(const std::string& name, const std::string& fontPath, float fontSize,
		TextRenderComponent::TextProvider provider, uint32_t canvasWidth = 512, uint32_t canvasHeight = 32);

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

	// HUD1種類分の定義。providerは[this]をキャプチャするラムダで、camera_/lastDeltaTime_等
	// 「呼び出し時点の最新値」をthis経由で毎回読むため、CreateHud時とLoad後の再バインド時とで
	// 同じインスタンスを使い回してよい（作り直す必要はない）
	struct HudDefinition {
		uint32_t canvasWidth;
		uint32_t canvasHeight;
		TextRenderComponent::TextProvider provider;
	};

	// HUD名→定義のテーブルを構築する。providerがcamera_/lastDeltaTime_等をキャプチャするため
	// インスタンスメソッドとして組み立てる。新しいHUDを追加したい場合はSceneBase.cppの
	// BuildHudDefinitions()に1エントリ足すだけでよく、CreateHud/RebindDynamicTextProvidersの
	// 分岐を増やす必要はない。呼び出し側はInitialize()で一度構築されるhudDefinitions_を使う
	// （毎回vector/ラムダを作り直さないように、テーブル自体はキャッシュする）
	std::vector<std::pair<std::string, HudDefinition>> BuildHudDefinitions();

	// BuildHudDefinitions()の結果をInitialize()で一度だけ構築してキャッシュしたもの
	std::vector<std::pair<std::string, HudDefinition>> hudDefinitions_;

	// TextProviderはラムダ（camera_等をキャプチャ）のためJSONに保存できない。Load直後は
	// dynamicTextなTextRenderComponentが空文字列のまま更新されなくなるため、名前で判別して
	// 対応するTextProviderを付け直す（hudDefinitions_のテーブルを参照する）
	void RebindDynamicTextProviders();

	// Objectsパネルの"Create HUD"ボタン用。HUD名からCreateDynamicTextObject呼び出しまでをまとめる
	// （hudDefinitions_のテーブルを参照する）
	void CreateHud(const std::string& hudName);

	// Render()の最後に更新する直近のdeltaTime（hudDefinitions_内のFPS用providerが参照する）
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
	void DrawHierarchy();
	void DrawInspector();

	// Unityの「Projectビュー」相当。ユーザーが追加した画像・音声・モデルをアイコンの一覧として
	// 表示し、ドラッグ&ドロップでオブジェクトへ付与できるようにする
	void DrawProjectPanel();

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
};
