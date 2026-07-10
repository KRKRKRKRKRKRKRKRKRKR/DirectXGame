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
#include "../Engine/GameObject/ComponentLoadContext.h"
#include "../Engine/GameObject/SceneSerializer.h"
#include <memory>
#include <vector>
#include <string>

// ゲームプレイ画面のワールド（GameObject群・当たり判定・Gizmo編集・ライティング）を保持するIScene実装
class PlayScene : public IScene {
public:
	void Initialize(Renderer* renderer, Camera* camera) override;
	void Render(float deltaTime) override;

	SceneType GetNextScene() const override { return nextScene_; }

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;
	SceneType nextScene_ = SceneType::kNone;

	// Unityの Play/Stop 相当。false(Stop)ではGameObject::Update()と押し戻しを止め、Gizmoで自由に配置できる
	bool isPlaying_ = false;

	// シーン内の全GameObject。unique_ptrで持つのは、vector再配置後もGameObject自体のアドレスが
	// 変わらないようにするため（gizmoTargets_/objectPanelTargets_が生ポインタで参照し続ける）
	std::vector<std::unique_ptr<GameObject>> objects_;

	// 新しいGameObjectを生成してobjects_へ追加し、安定した参照を返す
	GameObject& CreateObject(const std::string& name);

	// "Objects"パネルの"Create"から選べるアーキタイプ名の一覧と、選ばれた名前からGameObjectを
	// 1体生成する処理。中身はComponentRegistryが使うのと同じJSON形式のひな形をFromJsonに渡すだけ
	GameObject& CreateObjectFromArchetype(const std::string& archetypeName);

	// txt/フォントというファイルパスが要る点がModel/Spriteと同じでアーキタイプのコンボには
	// 載せられないため、専用の生成処理を用意する（Sprite2Dと同じスクリーン空間UI扱い）
	GameObject& CreateTextObject(const std::string& txtPath, const std::string& fontPath, float fontSize);

	// CreateTextObjectの動的テキスト版。GameObject生成・スクリーン空間設定・TextRenderComponent::
	// CreateDynamic呼び出し・SetTextProviderをまとめて行う（Camera座標HUD等、呼び出し側の
	// 引数を「何を・どのフォントで・何を表示するか」だけに絞るためのラッパー）
	GameObject& CreateDynamicTextObject(const std::string& name, const std::string& fontPath, float fontSize,
		TextRenderComponent::TextProvider provider, uint32_t canvasWidth = 512, uint32_t canvasHeight = 32);

	// Gizmoパネルで選択中のオブジェクトをobjects_から削除する
	void DeleteSelectedObject();

	// excludeFromGizmoList/excludeFromObjectPanelを見てgizmoTargets_/objectPanelTargets_を
	// objects_から作り直す。オブジェクトの生成・削除・ロードの後に必ず呼ぶ
	void RebuildDerivedLists();

	std::vector<TextureEntry> textures_;

	// Gizmo選択対象一覧（excludeFromGizmoList=falseのオブジェクトのみ）
	std::vector<GameObject*> gizmoTargets_;

	// "Objects"パネル表示用一覧（excludeFromObjectPanel=falseのオブジェクトのみ）
	std::vector<GameObject*> objectPanelTargets_;

	// スクリーン空間UI（TransformComponent::is2D==true）のマウス選択・Gizmo操作対象一覧。
	// gizmoTargets_（3D、ワールド空間レイキャスト）とは別に、GizmoControllerの2D版
	// （px座標のAABBピッキング＋正射影ギズモ）が扱う
	std::vector<GameObject*> screenTargets_;

	ColliderSystem colliderSystem_;
	GizmoController gizmoController_;

	void DrawGrid();
	void DrawImGui();
};
