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

	SceneType GetNextScene() const override { return nextScene_; }

protected:
	// SceneBase::Initializeの最後（RebuildDerivedLists()の直前）に呼ばれる。
	// 起動直後に生成しておきたいシーン固有のGameObject（HUD等）があれば派生クラスでオーバーライドする
	virtual void OnInitialize() {}

	// SceneBase::Renderの最後に呼ばれる。シーン固有のキー入力による遷移条件を
	// nextScene_への代入で表現する（デフォルトは何もしない＝遷移しない）
	virtual void HandleSceneTransitionInput() {}

	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	SceneType nextScene_ = SceneType::kNone;

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

	// "Objects"パネルの"Create"から選べるアーキタイプ名の一覧と、選ばれた名前からGameObjectを
	// 1体生成する処理。中身はComponentRegistryが使うのと同じJSON形式のひな形をFromJsonに渡すだけ。
	// customNameが空なら従来通り"アーキタイプ名 連番"を自動で付ける
	GameObject& CreateObjectFromArchetype(const std::string& archetypeName, const std::string& customName = "");

	// GameObject生成・スクリーン空間設定・TextRenderComponent::CreateDynamic呼び出し・
	// SetTextProviderをまとめて行う（Camera座標HUD等、呼び出し側の引数を
	// 「何を・どのフォントで・何を表示するか」だけに絞るためのラッパー）
	GameObject& CreateDynamicTextObject(const std::string& name, const std::string& fontPath, float fontSize,
		TextRenderComponent::TextProvider provider, uint32_t canvasWidth = 512, uint32_t canvasHeight = 32);

	// 説明文等の固定表示テキスト用。ImGuiで打ち込んだ内容をResources/Text/{name}.txtへ書き出してから
	// TextRenderComponent::CreateStaticでロードする（txtFilePathとしてJSONに保存されるため、
	// Save/Load後もそのまま同じファイルを読み直すだけで復元できる＝dynamicTextと違い再バインド不要）
	GameObject& CreateStaticTextObject(const std::string& name, const std::string& content, float fontSize = 32.0f);

	// Gizmoパネルで選択中のオブジェクトをobjects_から削除する
	void DeleteSelectedObject();

	// "Objects"パネルの"Add Component"セクション。selectedに対して、ComponentRegistryの
	// Simple系（引数なしで安全に追加できる型）はコンボ+Addボタンで一覧から、依存のある型
	// （Model/Sprite/TextureSelector/Mirror/AudioSource）は個別の入力欄+ボタンで追加する。
	// Cube等の完成形をArchetypeから1発生成する既存フローとは別に、空のGameObjectへ
	// 後から機能を積み上げていく生成スタイルを提供する
	void DrawAddComponentMenu(GameObject& selected);

	// objects_の保存/復元自体はSceneObjectStoreに委譲する（ファイルパス組み立て・
	// is2D振り分け・SceneSerializer呼び出しはそちらの責務）。ここではLoad後に必要な
	// Rebind/RebuildDerivedLists/ResetSelectionの後始末だけを行う
	void SaveScene();
	void LoadScene();

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

	// Unityの Scene/Game タブ相当。falseはエディタ自由カメラ+Gizmo（Scene）、
	// trueはシーン内カメラ視点でGizmoなし（Game）。ボタンでの切替はDrawImGui()内で行う
	bool viewingGameCamera_ = false;
};
