#include "PlayScene.h"
#include "../Externals/imgui/imgui.h"
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Math/VectorMath.h"
#include "../Math/JsonUtil.h"
#include "../Engine/Audio/AudioManager.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Engine/GameObject/ObjectArchetypes.h"
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <cstdio>

GameObject& PlayScene::CreateObject(const std::string& name) {
	auto obj = std::make_unique<GameObject>();
	obj->name = name;
	GameObject& ref = *obj;
	objects_.push_back(std::move(obj));
	return ref;
}

GameObject& PlayScene::CreateObjectFromArchetype(const std::string& archetypeName) {
	// 表示上の重複を避けるための簡易な連番付け（Save/LoadはもうGameObjectの名前一致に
	// 依存していないため、多少重複してもロード等の正しさには影響しない）
	std::string uniqueName = archetypeName + " " + std::to_string(objects_.size() + 1);
	GameObject& obj = CreateObject(uniqueName);
	ComponentLoadContext ctx{ renderer_, &textures_ };
	obj.FromJson(ObjectArchetypes::GetJson(archetypeName), ctx);
	return obj;
}

GameObject& PlayScene::CreateTextObject(const std::string& txtPath, const std::string& fontPath, float fontSize) {
	GameObject& obj = CreateObject("Text");
	// Sprite2Dと同じくスクリーン空間UIなのでGizmo選択対象からは外す
	obj.excludeFromGizmoList = true;
	obj.GetTransform().translation = { 50.0f, 50.0f, 0.0f };

	// is2D設定・箱の初期scale・Load()呼び出しは全部TextRenderComponent::CreateStatic側の責務
	TextRenderComponent::CreateStatic(obj, renderer_, txtPath, fontPath, fontSize);

	return obj;
}

GameObject& PlayScene::CreateDynamicTextObject(const std::string& name, const std::string& fontPath, float fontSize,
	TextRenderComponent::TextProvider provider, uint32_t canvasWidth, uint32_t canvasHeight) {
	GameObject& obj = CreateObject(name);
	obj.excludeFromGizmoList = true; // Sprite2Dと同じくスクリーン空間UIなのでGizmo選択対象からは外す
	obj.GetTransform().translation = { 10.0f, 10.0f, 0.0f };

	TextRenderComponent* text = TextRenderComponent::CreateDynamic(obj, renderer_, fontPath, fontSize, canvasWidth, canvasHeight);
	text->SetTextProvider(std::move(provider));

	return obj;
}

void PlayScene::DeleteSelectedObject() {
	GameObject* selected = gizmoController_.GetSelected(gizmoTargets_);
	if (!selected) selected = gizmoController_.GetSelected2D(screenTargets_);
	if (!selected) return;

	auto it = std::find_if(objects_.begin(), objects_.end(),
		[selected](const std::unique_ptr<GameObject>& obj) { return obj.get() == selected; });
	if (it != objects_.end()) objects_.erase(it);

	RebuildDerivedLists();
	gizmoController_.ResetSelection();
}

void PlayScene::RebuildDerivedLists() {
	gizmoTargets_.clear();
	objectPanelTargets_.clear();
	screenTargets_.clear();
	for (auto& obj : objects_) {
		if (!obj->excludeFromGizmoList) gizmoTargets_.push_back(obj.get());
		if (!obj->excludeFromObjectPanel) objectPanelTargets_.push_back(obj.get());
		if (obj->GetComponent<TransformComponent>()->is2D) screenTargets_.push_back(obj.get());
	}
}

void PlayScene::Initialize(Renderer* renderer, Camera* camera) {
	renderer_ = renderer;
	camera_ = camera;
	nextScene_ = SceneType::kNone;

	// テクスチャ一覧をまとめてロード（"なし" は白テクスチャ）
	textures_.push_back({ kTextureNone, "なし" });
	for (const std::string& path : std::vector<std::string>{
		"Resources/t.png",
		"Resources/f.png",
		"Resources/s.png",
		"Resources/monsterBall.png.png",
		"Resources/White.png",
		"Resources/transparent .png", // αTest確認用（板の隙間が透過になっている柵）
		}) {
		TextureHandle h = renderer_->LoadTexture(path);
		textures_.push_back({ h, path.substr(path.find_last_of('/') + 1) });
	}

	// 動作確認用のサンプルTextオブジェクト。Resources/Font/にフォントファイルが無い場合、
	// TextRenderComponent::Load()は失敗して何も描画しない（ビルド・起動自体は通る）
	CreateTextObject("Resources/Text/sample.txt", "Resources/Font/font.ttf", 32.0f);

	// Camera座標を毎フレーム表示するHUD。表示内容はTextProviderとして1回登録するだけで、
	// 以降はRender()内の汎用ループが毎フレーム自動的にUpdateDynamicText()を呼んでくれる
	// （GameObject生成・スクリーン空間設定・CreateDynamic呼び出しはCreateDynamicTextObject側にまとめてある）
	CreateDynamicTextObject("Camera Coord", "Resources/Font/font.ttf", 20.0f, [this]() {
		const Vector3& camPos = camera_->GetCameraData().position;
		char buf[128];
		std::snprintf(buf, sizeof(buf), "Camera: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
		return std::string(buf);
	});

	RebuildDerivedLists();
}

void PlayScene::Render(float deltaTime) {
	// ImGui::NewFrame()の後、ImGui::Render()の前に呼ぶ必要がある
	ImGuizmo::BeginFrame();

	// isPlaying_中のみコンポーネント更新（Stop中はGizmoで自由に配置できるようにする）
	if (isPlaying_) {
		for (auto* obj : gizmoTargets_) obj->Update(deltaTime);
	}

	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));

	// TextProviderを持つdynamicTextを毎フレーム更新する（Camera座標HUD等）。同じテクスチャの
	// 中身を書き換えるだけなので、新しいテクスチャハンドルを発行せず毎フレーム呼んでも枯渇しない。
	// HUDが増えてもこのループは変更不要（各Textが自分のTextProviderを持っているだけ）
	for (auto& obj : objects_) {
		if (auto* text = obj->GetComponent<TextRenderComponent>()) {
			text->UpdateDynamicText(renderer_);
		}
	}

	// Mirrorを探す（削除されていれば存在しない＝反射パス自体を丸ごとスキップする）
	MirrorComponent* mirror = nullptr;
	GameObject* mirrorObject = nullptr;
	for (auto& obj : objects_) {
		if (auto* m = obj->GetComponent<MirrorComponent>()) {
			mirror = m;
			mirrorObject = obj.get();
			break;
		}
	}

	if (mirror) {
		// 鏡の反射視点で、Mirror自身とSprite2D（スクリーン空間UI）以外を全部オフスクリーンへ描画する
		Collision::Plane mirrorPlane   = mirror->ComputePlane(mirrorObject->GetTransform());
		Matrix4x4        reflection    = MatrixMath::MakeReflectionMatrix(mirrorPlane);
		Matrix4x4        reflectedView = reflection * view;
		Vector3          reflectedCamPos = TransformMath::Transform(camera_->GetCameraData().position, reflection);

		renderer_->BeginMirrorPass(reflectedView, proj, reflectedCamPos);
		for (auto& obj : objects_) {
			if (obj->GetComponent<MirrorComponent>()) continue;
			if (auto* sprite = obj->GetComponent<SpriteRenderComponent>()) {
				if (!sprite->is3D) continue; // Sprite2Dは反射に映さない
			}
			if (auto* r = obj->GetComponent<RenderComponentBase>()) {
				// deltaTime=0：ModelRenderComponentのアニメーションを通常パスと二重に進めないため
				r->Draw(renderer_, obj->GetTransform(), 0.0f);
			}
		}
		renderer_->EndMirrorPass();
	}

	// ---- 通常パス ----
	renderer_->SetCamera(view, proj, camera_->GetCameraData().position);

	gizmoController_.UpdatePicking(gizmoTargets_, renderer_, view, proj);
	gizmoController_.UpdateGizmo(gizmoTargets_, renderer_, view, proj);
	gizmoController_.UpdatePicking2D(screenTargets_);
	gizmoController_.UpdateGizmo2D(screenTargets_, renderer_);

	for (auto& obj : objects_) {
		if (obj->GetComponent<MirrorComponent>()) continue; // Mirrorは反射テクスチャ確定後に描画する
		if (auto* r = obj->GetComponent<RenderComponentBase>()) {
			r->Draw(renderer_, obj->GetTransform(), deltaTime);
		}
	}

	if (mirror) {
		mirror->Draw(renderer_, mirrorObject->GetTransform());
	}

	colliderSystem_.ResolveAndDraw(gizmoTargets_, isPlaying_, renderer_, view, proj);

	// 各ライトコンポーネントが自分のSetter呼び出しとデバッグ表示を行う（ILightComponent経由で汎用処理）
	for (auto& obj : objects_) {
		if (auto* light = obj->GetComponent<ILightComponent>()) {
			light->SyncToRenderer(renderer_, obj->GetTransform());
			light->DrawGizmoVisualization(renderer_, obj->GetTransform(), view, proj);
		}
	}

	DrawImGui();

	// デバッグ用キー割り当て（ESCでTitle、F1でGameOverへ遷移）
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = SceneType::kTitle;
	if (Input::IsTriggered(DIK_F1))     nextScene_ = SceneType::kGameOver;
}

void PlayScene::DrawGrid() {
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

void PlayScene::DrawImGui() {
	ImGui::Begin("Gizmo");

	// Stop中はGravityComponent等の更新と押し戻しを止める
	if (isPlaying_) {
		if (ImGui::Button("Stop")) isPlaying_ = false;
	} else {
		if (ImGui::Button("Play")) isPlaying_ = true;
	}
	ImGui::SameLine();
	ImGui::Text(isPlaying_ ? "(Playing)" : "(Stopped)");
	ImGui::Separator();

	// Target/Edit Collider/操作モードはGizmoControllerが描画
	gizmoController_.DrawImGui(gizmoTargets_);
	gizmoController_.DrawImGui2D(screenTargets_);

	if (ImGui::Button("Delete Selected")) DeleteSelectedObject();

	ImGui::Separator();
	static const std::string kScenePath = "Resources/scene.json";
	if (ImGui::Button("Save")) SceneSerializer::Save(kScenePath, objects_);
	ImGui::SameLine();
	if (ImGui::Button("Load")) {
		ComponentLoadContext ctx{ renderer_, &textures_ };
		if (SceneSerializer::Load(kScenePath, objects_, ctx)) {
			RebuildDerivedLists();
			gizmoController_.ResetSelection();
		}
	}

	ImGui::End();

	ImGui::Begin("Objects");

	// "Create"：選んだアーキタイプのGameObjectを1体生成してobjects_へ追加する
	static int archetypeIndex = 0;
	const std::vector<std::string>& archetypeNameList = ObjectArchetypes::GetNames();
	std::vector<const char*> archetypeNames;
	for (auto& n : archetypeNameList) archetypeNames.push_back(n.c_str());
	ImGui::Combo("Archetype", &archetypeIndex, archetypeNames.data(), static_cast<int>(archetypeNames.size()));
	ImGui::SameLine();
	if (ImGui::Button("Create")) {
		CreateObjectFromArchetype(archetypeNameList[archetypeIndex]);
		RebuildDerivedLists();
	}
	ImGui::Separator();

	// Text：txt/フォントというファイルパスが要るためArchetypeコンボには載せず専用UIで作成する
	static char textTxtPathBuf[256]  = "Resources/Text/sample.txt";
	static char textFontPathBuf[256] = "Resources/Font/font.ttf";
	static float textFontSize = 32.0f;
	ImGui::InputText("Text Path", textTxtPathBuf, sizeof(textTxtPathBuf));
	ImGui::InputText("Font Path", textFontPathBuf, sizeof(textFontPathBuf));
	ImGui::DragFloat("Font Size", &textFontSize, 1.0f, 8.0f, 256.0f);
	if (ImGui::Button("Create Text")) {
		CreateTextObject(textTxtPathBuf, textFontPathBuf, textFontSize);
		RebuildDerivedLists();
	}
	ImGui::Separator();

	bool firstObject = true;
	for (auto* obj : objectPanelTargets_) {
		if (!firstObject) ImGui::Separator();
		firstObject = false;
		obj->DrawImGui();
	}
	ImGui::End();

	// 共有メッシュのグローバル設定はRenderer自身が描画する
	renderer_->DrawImGui();

	AudioManager::GetInstance().DrawImGui();

	// シーン全体の設定はSceneLight自身が描画する
	renderer_->GetLight().DrawImGui();
}
