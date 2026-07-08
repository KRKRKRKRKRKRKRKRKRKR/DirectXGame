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

	ColliderSystem colliderSystem_;
	GizmoController gizmoController_;

	// 固定パス（Resources/scene.json）へobjects_全体を保存/復元する。Loadはobjects_を
	// 一度全部破棄してJSONの内容から丸ごと再構築するため、保存時と異なる数でも復元できる
	void SaveScene(const std::string& path);
	void LoadScene(const std::string& path);

	void DrawGrid();
	void DrawImGui();
};
