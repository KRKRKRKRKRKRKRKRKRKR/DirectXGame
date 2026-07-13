#include "SceneBase.h"
#include "../Externals/imgui/imgui.h"
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Math/VectorMath.h"
#include "../Math/JsonUtil.h"
#include "../Engine/Audio/AudioManager.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Engine/GameObject/ObjectArchetypes.h"
#include "../Engine/Utils/StringUtils.h"
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <fstream>

GameObject& SceneBase::CreateObject(const std::string& name) {
	auto obj = std::make_unique<GameObject>();
	obj->name = name;
	GameObject& ref = *obj;
	objects_.push_back(std::move(obj));
	return ref;
}

GameObject& SceneBase::CreateObjectFromArchetype(const std::string& archetypeName, const std::string& customName) {
	// customName未指定時は表示上の重複を避けるための簡易な連番付け（Save/LoadはもうGameObjectの
	// 名前一致に依存していないため、多少重複してもロード等の正しさには影響しない）
	std::string name = customName.empty() ? (archetypeName + " " + std::to_string(objects_.size() + 1)) : customName;
	GameObject& obj = CreateObject(name);
	ComponentLoadContext ctx{ renderer_, &textures_ };
	obj.FromJson(ObjectArchetypes::GetJson(archetypeName), ctx);

	// Unityの「GameObject作成メニューはシーンビューカメラの前方に生成する」動作を再現する。
	// 常に原点付近の固定座標だと連続生成のたびに完全に重なって見分けが付かないため、
	// 現在のカメラ位置から一定距離前方に置く（Unityと同じく他オブジェクトとの重なり自体は許容する）
	constexpr float kSpawnDistance = 5.0f;
	Vector3 forward = TransformMath::EulerRadiansToDirection(camera_->GetCameraData().rotation);
	obj.GetTransform().translation = camera_->GetCameraData().position + forward * kSpawnDistance;

	RebuildDerivedLists();
	return obj;
}

GameObject& SceneBase::CreateDynamicTextObject(const std::string& name, const std::string& fontPath, float fontSize,
	TextRenderComponent::TextProvider provider, uint32_t canvasWidth, uint32_t canvasHeight) {
	GameObject& obj = CreateObject(name);
	obj.excludeFromGizmoList = true; // Sprite2Dと同じくスクリーン空間UIなのでGizmo選択対象からは外す
	obj.GetTransform().translation = { 10.0f, 10.0f, 0.0f };

	TextRenderComponent* text = TextRenderComponent::CreateDynamic(obj, renderer_, fontPath, fontSize, canvasWidth, canvasHeight);
	text->SetTextProvider(std::move(provider));

	return obj;
}

GameObject& SceneBase::CreateStaticTextObject(const std::string& name, const std::string& content, float fontSize) {
	// Name欄をそのままファイル名に使う（Resources/{assetFolder_}/Text/{name}.txt）。同名で既に
	// 存在するファイルは上書きする（説明文の言い直し・作り直しは同じ名前で作れば内容だけ更新される
	// 想定）。assetFolder_で分けることで、シーンをまたいで同名の説明文を作っても他シーンの
	// ファイルを上書きしない（UI/Objectの分割保存と同じ理由でシーンごとに独立させる）。
	// nameはImGuiのInputTextからのUTF-8文字列のため、そのままstd::ofstream(std::string)に渡すと
	// Windowsの実行時ロケール（日本語環境ならShift-JIS）としてパスが解釈され、日本語ファイル名が
	// 文字化けする。std::wstring版のパスに変換してから渡すことでUTF-8のまま正しく扱われる。
	// また"/"・"\"・".."をそのまま許すとResources/Text/配下から外れた場所へ書き込めてしまうため、
	// パス区切り文字は取り除いてから使う（意図しないディレクトリへの書き込みを防ぐ）
	std::string sanitizedName = name;
	sanitizedName.erase(std::remove_if(sanitizedName.begin(), sanitizedName.end(),
		[](char c) { return c == '/' || c == '\\' || c == ':'; }), sanitizedName.end());
	std::string txtPath = "Resources/" + assetFolder_ + "/Text/" + sanitizedName + ".txt";
	std::ofstream file(StringUtils::ConvertString(txtPath), std::ios::binary);
	file << content;
	file.close();

	GameObject& obj = CreateObject(name);
	// 2Dスクリーン空間UIだがGizmoで自由に配置したいため、excludeFromGizmoListはfalseのまま
	// （is2D==trueなのでUpdatePicking2D/UpdateGizmo2D側の対象になる）
	obj.GetTransform().translation = { 10.0f, 10.0f, 0.0f };

	TextRenderComponent::CreateStatic(obj, renderer_, txtPath, "Resources/Font/font.ttf", fontSize);

	RebuildDerivedLists();
	return obj;
}

void SceneBase::SaveScene() {
	SceneObjectStore::Save(assetFolder_, objects_);
}

void SceneBase::LoadScene() {
	ComponentLoadContext ctx{ renderer_, &textures_ };
	if (SceneObjectStore::Load(assetFolder_, objects_, ctx)) {
		RebindDynamicTextProviders();
		RebuildDerivedLists();
		gizmoController_.ResetSelection();
	}
}

void SceneBase::DeleteSelectedObject() {
	GameObject* selected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	if (!selected) return;

	auto it = std::find_if(objects_.begin(), objects_.end(),
		[selected](const std::unique_ptr<GameObject>& obj) { return obj.get() == selected; });
	if (it != objects_.end()) objects_.erase(it);

	RebuildDerivedLists();
	gizmoController_.ResetSelection();
}

void SceneBase::RebuildDerivedLists() {
	gizmoTargets_.clear();
	screenTargets_.clear();
	for (auto& obj : objects_) {
		if (!obj->excludeFromGizmoList) gizmoTargets_.push_back(obj.get());
		if (obj->GetComponent<TransformComponent>()->is2D) screenTargets_.push_back(obj.get());
	}
}

void SceneBase::Initialize(Renderer* renderer, Camera* camera, const std::string& assetFolder) {
	renderer_ = renderer;
	camera_ = camera;
	assetFolder_ = assetFolder;
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

	// HUDテーブルはcamera_等をキャプチャするラムダを含むため、Initialize後の状態で一度だけ
	// 構築してキャッシュする（CreateHud/RebindDynamicTextProviders/DrawImGuiが毎回作り直さない）
	hudDefinitions_ = BuildHudDefinitions();

	OnInitialize(); // シーン固有の初期HUD等はここで生成する

	RebuildDerivedLists();
}

std::vector<std::pair<std::string, SceneBase::HudDefinition>> SceneBase::BuildHudDefinitions() {
	// 新しいHUDを追加する場合はここに1エントリ足すだけでよい（CreateHud/RebindDynamicTextProviders
	// 側の分岐は変更不要）
	return {
		{ "Camera Coord", HudDefinition{ 512, 64, [this]() {
			const Vector3& camPos = camera_->GetCameraData().position;
			char buf[128];
			std::snprintf(buf, sizeof(buf), "Camera: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
			return std::string(buf);
		} } },
		{ "FPS", HudDefinition{ 256, 64, [this]() {
			// 0除算を避ける（起動直後の1フレーム目はlastDeltaTime_が0のまま呼ばれる可能性がある）
			float fps = (lastDeltaTime_ > 0.0f) ? (1.0f / lastDeltaTime_) : 0.0f;
			char buf[64];
			std::snprintf(buf, sizeof(buf), "FPS: %.1f", fps);
			return std::string(buf);
		} } },
	};
}

void SceneBase::CreateHud(const std::string& hudName) {
	for (auto& entry : hudDefinitions_) {
		if (entry.first != hudName) continue;
		const HudDefinition& def = entry.second;
		CreateDynamicTextObject(hudName, "Resources/Font/font.ttf", 20.0f, def.provider, def.canvasWidth, def.canvasHeight);
		RebuildDerivedLists();
		return;
	}
}

void SceneBase::RebindDynamicTextProviders() {
	// TextProviderはラムダのためJSONに保存できず、Load直後は空になっている。
	// 名前で該当オブジェクトを見つけて対応するProviderを付け直す
	for (auto& obj : objects_) {
		auto* text = obj->GetComponent<TextRenderComponent>();
		if (!text || !text->dynamicText) continue;
		for (auto& entry : hudDefinitions_) {
			if (entry.first != obj->name) continue;
			text->SetTextProvider(entry.second.provider);
			break;
		}
	}
}

void SceneBase::Render(float deltaTime) {
	lastDeltaTime_ = deltaTime; // hudDefinitions_内のFPS用providerが参照する直近のフレーム時間

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
	gizmoController_.UpdatePicking2D(screenTargets_, renderer_);
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

	// シーン遷移条件は派生クラスごとに異なるため、ここでフックへ委譲する
	HandleSceneTransitionInput();
}

void SceneBase::DrawGrid() {
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

void SceneBase::DrawImGui() {
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
	// UI（is2D）とObject（3D）をResources/{assetFolder_}/ui.json・scene.jsonの2ファイルに分けて保存/復元する
	if (ImGui::Button("Save")) SaveScene();
	ImGui::SameLine();
	if (ImGui::Button("Load")) LoadScene();

	ImGui::End();

	ImGui::Begin("Objects");

	// "Create"：選んだアーキタイプのGameObjectを1体生成してobjects_へ追加する。
	// Name欄が空なら従来通り"アーキタイプ名 連番"の自動名になる
	static int archetypeIndex = 0;
	static char createNameBuf[128] = "";
	const std::vector<std::string>& archetypeNameList = ObjectArchetypes::GetNames();
	// ObjectArchetypes::GetNames()は固定リストのため、const char*一覧も初回だけ構築すればよい
	static std::vector<const char*> archetypeNames = [&archetypeNameList]() {
		std::vector<const char*> names;
		for (auto& n : archetypeNameList) names.push_back(n.c_str());
		return names;
	}();
	ImGui::Combo("Archetype", &archetypeIndex, archetypeNames.data(), static_cast<int>(archetypeNames.size()));
	ImGui::InputTextWithHint("Name", "(未入力なら自動命名)", createNameBuf, sizeof(createNameBuf));
	if (ImGui::Button("Create")) {
		CreateObjectFromArchetype(archetypeNameList[archetypeIndex], createNameBuf);
		createNameBuf[0] = '\0';
	}
	ImGui::Separator();

	// "Create HUD"：hudDefinitions_（Initialize時に一度構築済み）に登録されているHUDを
	// コンボから選んで追加する（一覧はテーブル駆動、HUDを増やしてもここは変更不要）
	static int hudIndex = 0;
	std::vector<const char*> hudNamesRaw;
	for (auto& entry : hudDefinitions_) hudNamesRaw.push_back(entry.first.c_str());
	ImGui::Combo("HUD", &hudIndex, hudNamesRaw.data(), static_cast<int>(hudNamesRaw.size()));
	ImGui::SameLine();
	if (ImGui::Button("Create HUD") && hudIndex >= 0 && hudIndex < static_cast<int>(hudDefinitions_.size())) {
		CreateHud(hudDefinitions_[hudIndex].first);
	}
	ImGui::Separator();

	// "Create Text"：打ち込んだ説明文をResources/Text/{Name}.txtへ保存し、静的テキストとして生成する。
	// Nameが空のままだと保存先ファイル名が決まらないため、その場合はボタンを無効化する
	static char staticTextNameBuf[128] = "";
	static char staticTextContentBuf[1024] = "";
	ImGui::InputText("Text Name", staticTextNameBuf, sizeof(staticTextNameBuf));
	ImGui::InputTextMultiline("Content", staticTextContentBuf, sizeof(staticTextContentBuf), ImVec2(0, 80));
	bool canCreateText = staticTextNameBuf[0] != '\0';
	if (!canCreateText) ImGui::BeginDisabled();
	if (ImGui::Button("Create Text")) {
		CreateStaticTextObject(staticTextNameBuf, staticTextContentBuf);
		staticTextNameBuf[0] = '\0';
		staticTextContentBuf[0] = '\0';
	}
	if (!canCreateText) ImGui::EndDisabled();
	ImGui::Separator();

	// Gizmoで選択中のオブジェクトのみ詳細を表示する（3D/2Dのうち最後に選んだ方を優先）
	GameObject* selected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	if (selected) {
		selected->DrawImGui();
	} else {
		ImGui::TextDisabled("(オブジェクト未選択)");
	}
	ImGui::End();

	// 共有メッシュのグローバル設定はRenderer自身が描画する
	renderer_->DrawImGui();

	AudioManager::GetInstance().DrawImGui();

	// シーン全体の設定はSceneLight自身が描画する
	renderer_->GetLight().DrawImGui();
}
