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
#include "../Engine/Utils/Logger.h"
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <format>

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

	// TODO(debug): テキスト非表示バグ調査用の一時ログ。原因特定後に削除する
	Logger::Log(std::format("CreateStaticTextObject: name='{}' objects_.size()={} before create\n", name, objects_.size()));

	// 同名の既存GameObject（静的テキスト）があれば、新規に重複したオブジェクトを作らず、
	// そのTextRenderComponentを読み直すだけにする（位置・箱サイズ等は現状のまま維持し、
	// 内容だけ更新される。以前は毎回新規生成していたため、同名で作り直すたびに古いオブジェクトが
	// 別の位置に残り続け、「テキストが消えたように見える」不具合の原因になっていた）
	for (auto& existing : objects_) {
		if (existing->name != name) continue;
		auto* text = existing->GetComponent<TextRenderComponent>();
		if (!text || text->dynamicText) continue; // dynamicText（HUD）は対象外、静的テキストのみ更新する
		text->fontSize = fontSize;
		bool ok = text->Load(renderer_);
		Logger::Log(std::format("CreateStaticTextObject: UPDATED existing '{}', Load()={}\n", name, ok));
		return *existing;
	}

	GameObject& obj = CreateObject(name);
	// 2Dスクリーン空間UIだがGizmoで自由に配置したいため、excludeFromGizmoListはfalseのまま
	// （is2D==trueなのでUpdatePicking2D/UpdateGizmo2D側の対象になる）
	obj.GetTransform().translation = { 10.0f, 10.0f, 0.0f };

	TextRenderComponent* text = TextRenderComponent::CreateStatic(obj, renderer_, txtPath, "Resources/Font/font.ttf", fontSize);
	Logger::Log(std::format("CreateStaticTextObject: CREATED new '{}', textureHandle={}, objects_.size()={} after\n",
		name, text->textureHandle, objects_.size()));

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
	// 矩形選択（複数選択）中はそちらを優先して一括削除する。矩形選択していなければ
	// 従来通りGizmoで選択中の1オブジェクトだけを消す
	const std::vector<GameObject*>& multiSelected = gizmoController_.GetMultiSelected2D();
	std::vector<GameObject*> roots;
	if (!multiSelected.empty()) {
		roots = multiSelected;
	} else {
		GameObject* selected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
		if (!selected) return;
		roots.push_back(selected);
	}

	// 削除対象は選択されたオブジェクト自身だけでなく、子孫もすべて含める（カスケード削除）。
	// 含めないと、生き残った親のchildren_が破棄済みポインタを指すダングリング状態になる
	std::vector<GameObject*> toDelete;
	std::vector<GameObject*> stack = roots;
	while (!stack.empty()) {
		GameObject* cur = stack.back();
		stack.pop_back();
		toDelete.push_back(cur);
		for (GameObject* child : cur->GetChildren()) stack.push_back(child);
	}

	// 生き残る親のchildren_から先に自分を取り除いておく（親が削除対象に含まれない場合の後始末）
	for (GameObject* obj : toDelete) obj->SetParent(nullptr);

	objects_.erase(
		std::remove_if(objects_.begin(), objects_.end(),
			[&toDelete](const std::unique_ptr<GameObject>& obj) {
				return std::find(toDelete.begin(), toDelete.end(), obj.get()) != toDelete.end();
			}),
		objects_.end());

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

	// Resources/{assetFolder_}/ui.json・scene.jsonが既に存在するなら、起動直後に自動で読み込む。
	// LoadScene()はファイルが無ければ何もせず終わる（SceneSerializer::Loadがfalseを返すだけ）ため、
	// 初回起動（保存済みファイルがまだ無い）でも安全に呼べる。存在する場合はOnInitialize()で
	// 作った内容（Camera Coord等）もLoadの結果で上書きされる
	LoadScene();

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
				r->Draw(renderer_, obj->GetWorldTransform(), 0.0f);
			}
		}
		renderer_->EndMirrorPass();
	}

	// シーン内で最初に見つかったCameraComponentを、Gameモード用の候補として探す
	// （Unityの「MainCameraタグ」相当は無く、機械的に最初の1つを使う）
	CameraComponent* gameCamera = nullptr;
	GameObject* gameCameraObject = nullptr;
	for (auto& obj : objects_) {
		if (auto* c = obj->GetComponent<CameraComponent>()) {
			gameCamera = c;
			gameCameraObject = obj.get();
			break;
		}
	}
	// カメラが無くなったら（削除された等）強制的にSceneへ戻す
	if (!gameCamera) viewingGameCamera_ = false;
	bool useGameCamera = viewingGameCamera_ && gameCamera != nullptr;

	// ---- メインパス：Scene（エディタカメラ+Gizmo）とGame（配置カメラ、Gizmoなし）は
	// 画面全体を共有し、ボタンでの選択に応じてどちらか一方だけを描画する ----
	Matrix4x4 activeView = view;
	Matrix4x4 activeProj = proj;
	Vector3   activeCamPos = camera_->GetCameraData().position;
	if (useGameCamera) {
		Transform camWorld = gameCameraObject->GetWorldTransform();
		activeView = gameCamera->GetViewMatrix(camWorld);
		activeProj = gameCamera->GetProjectionMatrix(
			camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
		activeCamPos = camWorld.translation;
	}
	renderer_->SetCamera(activeView, activeProj, activeCamPos);

	// Gizmoのピッキング/操作はScene表示中のみ（Game表示中は選択・編集させない）
	if (!useGameCamera) {
		gizmoController_.UpdatePicking(gizmoTargets_, renderer_, activeView, activeProj);
		gizmoController_.UpdateGizmo(gizmoTargets_, renderer_, activeView, activeProj);
		gizmoController_.UpdatePicking2D(screenTargets_, renderer_);
		gizmoController_.UpdateGizmo2D(screenTargets_, renderer_);
	}

	for (auto& obj : objects_) {
		if (obj->GetComponent<MirrorComponent>()) continue; // Mirrorは反射テクスチャ確定後に描画する
		if (auto* r = obj->GetComponent<RenderComponentBase>()) {
			r->Draw(renderer_, obj->GetWorldTransform(), deltaTime);
		}
	}

	if (mirror) {
		mirror->Draw(renderer_, mirrorObject->GetWorldTransform());
	}

	// 当たり判定の解決（押し戻し）自体はScene/Gameどちらの表示中でも行うが、
	// ワイヤーフレームのデバッグ描画はScene表示中のみ（drawDebug引数）
	colliderSystem_.ResolveAndDraw(gizmoTargets_, isPlaying_, renderer_, activeView, activeProj, !useGameCamera);

	// 各ライトコンポーネントが自分のSetter呼び出しとデバッグ表示を行う（ILightComponent経由で汎用処理）。
	// SyncToRenderer（実際のライティング反映）はどちらの表示中でも必要、デバッグ可視化はScene表示中のみ
	for (auto& obj : objects_) {
		if (auto* light = obj->GetComponent<ILightComponent>()) {
			light->SyncToRenderer(renderer_, obj->GetWorldTransform());
			if (!useGameCamera) {
				light->DrawGizmoVisualization(renderer_, obj->GetWorldTransform(), activeView, activeProj);
			}
		}
	}

	DrawImGui();

	// シーン遷移条件は派生クラスごとに異なるため、ここでフックへ委譲する。
	// ImGuiのテキスト入力欄等がキーボードを掴んでいる間はEnter/Escape等のホットキーを
	// 無視する（Inspectorの名前欄でEnterを押しただけでシーン遷移してしまう等を防ぐ）
	if (!ImGui::GetIO().WantCaptureKeyboard) {
		HandleSceneTransitionInput();
	}
}

void SceneBase::DrawGrid() {
	Matrix4x4 viewMatrix = camera_->GetViewMatrix();
	Matrix4x4 projMatrix = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetClientWidth(), renderer_->GetClientHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

void SceneBase::DrawAddComponentMenu(GameObject& selected) {
	if (!ImGui::CollapsingHeader("コンポーネントを追加")) return;

	ComponentLoadContext ctx{ renderer_, &textures_ };

	// ---- Simple系：ComponentRegistry::RegisterSimpleで登録済みの型を一覧化し、
	// 選んでAddボタンを押すだけで既定値のコンポーネントを追加できる ----
	const std::vector<std::string>& simpleNames = ComponentRegistry::GetSimpleTypeNames();
	static int simpleIndex = 0;
	// コンボの表示だけ日本語表示名にする（実際の追加/削除は英語のtypeNameであるsimpleNamesを使う）
	std::vector<std::string> simpleDisplayNames;
	for (auto& n : simpleNames) simpleDisplayNames.push_back(ComponentRegistry::GetDisplayName(n));
	std::vector<const char*> simpleNamesRaw;
	for (auto& n : simpleDisplayNames) simpleNamesRaw.push_back(n.c_str());
	if (!simpleNamesRaw.empty()) {
		if (simpleIndex >= static_cast<int>(simpleNamesRaw.size())) simpleIndex = 0;
		ImGui::Combo("コンポーネント", &simpleIndex, simpleNamesRaw.data(), static_cast<int>(simpleNamesRaw.size()));
		ImGui::SameLine();
		if (ImGui::Button("追加##SimpleComponent")) {
			ComponentRegistry::Create(simpleNames[simpleIndex], selected, ctx, nlohmann::json::object());
		}
		ImGui::SameLine();
		// コンボで選択中の型をこのGameObjectから取り除く。付いていない型を選んだ状態で押しても
		// ComponentRegistry::RemoveByTypeName側がfalseを返すだけで何も起きない
		if (ImGui::Button("削除##SimpleComponent")) {
			ComponentRegistry::RemoveByTypeName(simpleNames[simpleIndex], selected);
		}
	}

	ImGui::Separator();

	// ---- 依存あり系：コンストラクタ引数や兄弟コンポーネントが要るため個別UIを用意する ----

	// ModelRender：directoryPath/filenameを指定してLoadModelを呼び直す必要がある
	if (ImGui::TreeNode("モデル描画")) {
		static char modelDirBuf[256] = "Resources";
		static char modelFileBuf[256] = "";
		static bool modelHasAnimation = false;
		ImGui::InputText("ディレクトリ", modelDirBuf, sizeof(modelDirBuf));
		ImGui::InputText("ファイル名", modelFileBuf, sizeof(modelFileBuf));
		ImGui::Checkbox("アニメーションあり", &modelHasAnimation);
		bool canAdd = modelFileBuf[0] != '\0';
		if (!canAdd) ImGui::BeginDisabled();
		if (ImGui::Button("追加##ModelRender")) {
			nlohmann::json data;
			data["directoryPath"] = std::string(modelDirBuf);
			data["filename"] = std::string(modelFileBuf);
			data["hasAnimation"] = modelHasAnimation;
			ComponentRegistry::Create("ModelRender", selected, ctx, data);
			modelFileBuf[0] = '\0';
		}
		if (!canAdd) ImGui::EndDisabled();
		ImGui::SameLine();
		// 削除の可否判定（TextureSelector依存ガード込み）はComponentRegistryのremoverに一本化してある
		if (ImGui::Button("削除##ModelRender")) {
			ComponentRegistry::RemoveByTypeName("ModelRender", selected);
		}
		ImGui::TreePop();
	}

	// SpriteRender：is3Dのみコンストラクタ引数。テクスチャ自体はTextureSelectorComponentを
	// 別途追加して選ぶ運用（RenderComponentBase::textureHandleは初期値kTextureNoneのまま）。
	// is3D=false（2D UI）を選んだ場合はTransformComponent::is2Dも合わせてtrueにし、
	// TextRenderComponent::CreateStatic等と同じくスクリーン空間ピクセル座標として扱えるようにする
	if (ImGui::TreeNode("スプライト描画")) {
		static bool spriteIs3D = true;
		ImGui::Checkbox("3D", &spriteIs3D);
		if (ImGui::Button("追加##SpriteRender")) {
			nlohmann::json data;
			data["is3D"] = spriteIs3D;
			ComponentRegistry::Create("SpriteRender", selected, ctx, data);
			if (!spriteIs3D) {
				TransformComponent* transform = selected.GetComponent<TransformComponent>();
				transform->is2D = true;
				transform->translationSpeed = 1.0f; transform->translationMin = 0.0f; transform->translationMax = 1920.0f;
				transform->scaleSpeed = 1.0f; transform->scaleMin = 1.0f; transform->scaleMax = 1920.0f;
				RebuildDerivedLists(); // is2Dが変わったのでscreenTargets_に反映させる
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("削除##SpriteRender")) {
			ComponentRegistry::RemoveByTypeName("SpriteRender", selected);
		}
		ImGui::TreePop();
	}

	// TextureSelector：同じGameObjectに既にRenderComponentBase系（CubeRender/SphereRender/
	// SpriteRender等）が付いていることが前提。無ければ追加できないようにする
	if (ImGui::TreeNode("テクスチャ選択")) {
		bool hasRenderComponent = selected.GetComponent<RenderComponentBase>() != nullptr;
		if (!hasRenderComponent) {
			ImGui::TextDisabled("(先にCube/Sphere/Sprite Render等を追加してください)");
		} else if (textures_.empty()) {
			ImGui::TextDisabled("(利用可能なテクスチャがありません)");
		} else {
			if (ImGui::Button("追加##TextureSelector")) {
				nlohmann::json data;
				data["textureName"] = textures_[0].name;
				ComponentRegistry::Create("TextureSelector", selected, ctx, data);
			}
		}
		if (selected.GetComponent<TextureSelectorComponent>()) {
			ImGui::SameLine();
			if (ImGui::Button("削除##TextureSelector")) {
				ComponentRegistry::RemoveByTypeName("TextureSelector", selected);
			}
		}
		ImGui::TreePop();
	}

	// Mirror：同じGameObjectに既にCubeRenderComponentが付いていることが前提
	if (ImGui::TreeNode("鏡")) {
		bool hasCubeRender = selected.GetComponent<CubeRenderComponent>() != nullptr;
		if (!hasCubeRender) {
			ImGui::TextDisabled("(先にCube Renderを追加してください)");
		} else if (ImGui::Button("追加##Mirror")) {
			ComponentRegistry::Create("Mirror", selected, ctx, nlohmann::json::object());
		}
		if (selected.GetComponent<MirrorComponent>()) {
			ImGui::SameLine();
			if (ImGui::Button("削除##Mirror")) {
				ComponentRegistry::RemoveByTypeName("Mirror", selected);
			}
		}
		ImGui::TreePop();
	}

	// AudioSource：filePath/registeredName/type/loopを指定してSound::Loadを呼び直す必要がある
	if (ImGui::TreeNode("オーディオソース")) {
		static char audioPathBuf[256] = "";
		static char audioNameBuf[128] = "";
		static int  audioTypeIndex = 0; // 0=BGM, 1=SE
		static bool audioLoop = true;
		const char* typeNames[] = { "BGM", "SE" };
		ImGui::InputText("ファイルパス", audioPathBuf, sizeof(audioPathBuf));
		ImGui::InputText("登録名", audioNameBuf, sizeof(audioNameBuf));
		ImGui::Combo("サウンド種別", &audioTypeIndex, typeNames, 2);
		ImGui::Checkbox("ループ", &audioLoop);
		bool canAdd = audioPathBuf[0] != '\0' && audioNameBuf[0] != '\0';
		if (!canAdd) ImGui::BeginDisabled();
		if (ImGui::Button("追加##AudioSource")) {
			nlohmann::json data;
			data["filePath"] = std::string(audioPathBuf);
			data["registeredName"] = std::string(audioNameBuf);
			data["soundType"] = audioTypeIndex;
			data["loop"] = audioLoop;
			ComponentRegistry::Create("AudioSource", selected, ctx, data);
			audioPathBuf[0] = '\0';
			audioNameBuf[0] = '\0';
		}
		if (!canAdd) ImGui::EndDisabled();
		if (selected.GetComponent<AudioSourceComponent>()) {
			ImGui::SameLine();
			if (ImGui::Button("削除##AudioSource")) {
				ComponentRegistry::RemoveByTypeName("AudioSource", selected);
			}
		}
		ImGui::TreePop();
	}
}

void SceneBase::DrawImGui() {
	ImGui::Begin("ギズモ##Gizmo");

	// Stop中はGravityComponent等の更新と押し戻しを止める
	if (isPlaying_) {
		if (ImGui::Button("停止")) isPlaying_ = false;
	} else {
		if (ImGui::Button("再生")) isPlaying_ = true;
	}
	ImGui::SameLine();
	ImGui::Text(isPlaying_ ? "（再生中）" : "（停止中）");

	// Scene/Gameビュー切替（Unityのタブ相当をボタンで実現）。画面全体はどちらか一方しか
	// 表示しない設計のため、シーン内にCameraComponentが無ければGameボタンは無効化する
	bool hasGameCamera = false;
	for (auto& obj : objects_) {
		if (obj->GetComponent<CameraComponent>()) { hasGameCamera = true; break; }
	}
	if (!hasGameCamera) viewingGameCamera_ = false;

	ImGui::BeginDisabled(!viewingGameCamera_);
	if (ImGui::Button("Scene")) viewingGameCamera_ = false;
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!hasGameCamera || viewingGameCamera_);
	if (ImGui::Button("Game")) viewingGameCamera_ = true;
	ImGui::EndDisabled();
	if (!hasGameCamera) {
		ImGui::SameLine();
		ImGui::TextDisabled("（カメラなし）");
	}
	ImGui::Separator();

	// Edit Collider/操作モードはGizmoControllerが描画（オブジェクト選択自体はHierarchyパネルへ一本化済み）
	gizmoController_.DrawImGui(gizmoTargets_);

	if (ImGui::Button("選択を削除")) DeleteSelectedObject();

	ImGui::Separator();
	// UI（is2D）とObject（3D）をResources/{assetFolder_}/ui.json・scene.jsonの2ファイルに分けて保存/復元する
	if (ImGui::Button("保存")) SaveScene();
	ImGui::SameLine();
	if (ImGui::Button("読み込み")) LoadScene();

	ImGui::Separator();
	// シーン遷移。今まではキーボード（Enter/Escape/F1等）のみだったため、ImGuiからも
	// 直接切り替えられるようにする。ボタンはnextScene_へ代入するだけで、実際の切替は
	// 既存のSceneManager::Render()内（GetNextScene()を見てChangeScene）で行われる
	ImGui::Text("シーン切替");
	if (ImGui::Button("タイトル")) nextScene_ = SceneType::kTitle;
	ImGui::SameLine();
	if (ImGui::Button("セレクト")) nextScene_ = SceneType::kSelect;
	ImGui::SameLine();
	if (ImGui::Button("プレイ")) nextScene_ = SceneType::kPlay;
	ImGui::SameLine();
	if (ImGui::Button("ゲームオーバー")) nextScene_ = SceneType::kGameOver;

	ImGui::End();

	DrawHierarchy();
	DrawInspector();

	ImGui::Begin("オブジェクト##Objects");

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
	ImGui::Combo("アーキタイプ", &archetypeIndex, archetypeNames.data(), static_cast<int>(archetypeNames.size()));
	ImGui::InputTextWithHint("名前", "(未入力なら自動命名)", createNameBuf, sizeof(createNameBuf));
	if (ImGui::Button("作成")) {
		CreateObjectFromArchetype(archetypeNameList[archetypeIndex], createNameBuf);
		createNameBuf[0] = '\0';
	}
	ImGui::SameLine();
	// Archetype（完成形）を1発生成する上のCreateとは別に、TransformComponentのみの空オブジェクトを
	// 生成する。中身はAdd Componentメニューから後付けで組み立てる運用向け
	if (ImGui::Button("空オブジェクトを作成")) {
		std::string name = createNameBuf[0] != '\0' ? std::string(createNameBuf) : ("空オブジェクト " + std::to_string(objects_.size() + 1));
		CreateObject(name);
		createNameBuf[0] = '\0';
		RebuildDerivedLists();
	}
	ImGui::Separator();

	// "Create HUD"：hudDefinitions_（Initialize時に一度構築済み）に登録されているHUDを
	// コンボから選んで追加する（一覧はテーブル駆動、HUDを増やしてもここは変更不要）
	static int hudIndex = 0;
	// コンボの表示だけ日本語にする。CreateHud等が使う内部キー（entry.first）はSave済みシーンの
	// 動的テキスト再バインド（RebindDynamicTextProviders）に使われるため英語のまま変更しない
	std::vector<std::string> hudDisplayNames;
	for (auto& entry : hudDefinitions_) {
		hudDisplayNames.push_back(entry.first == "Camera Coord" ? "カメラ座標" : entry.first);
	}
	std::vector<const char*> hudNamesRaw;
	for (auto& n : hudDisplayNames) hudNamesRaw.push_back(n.c_str());
	ImGui::Combo("HUD", &hudIndex, hudNamesRaw.data(), static_cast<int>(hudNamesRaw.size()));
	ImGui::SameLine();
	if (ImGui::Button("HUDを作成") && hudIndex >= 0 && hudIndex < static_cast<int>(hudDefinitions_.size())) {
		CreateHud(hudDefinitions_[hudIndex].first);
	}
	ImGui::Separator();

	// "Create Text"：打ち込んだ説明文をResources/Text/{Name}.txtへ保存し、静的テキストとして生成する。
	// Nameが空のままだと保存先ファイル名が決まらないため、その場合はボタンを無効化する
	static char staticTextNameBuf[128] = "";
	static char staticTextContentBuf[1024] = "";
	ImGui::InputText("テキスト名", staticTextNameBuf, sizeof(staticTextNameBuf));
	ImGui::InputTextMultiline("内容", staticTextContentBuf, sizeof(staticTextContentBuf), ImVec2(0, 80));
	bool canCreateText = staticTextNameBuf[0] != '\0';
	if (!canCreateText) ImGui::BeginDisabled();
	if (ImGui::Button("テキストを作成")) {
		CreateStaticTextObject(staticTextNameBuf, staticTextContentBuf);
		staticTextNameBuf[0] = '\0';
		staticTextContentBuf[0] = '\0';
	}
	if (!canCreateText) ImGui::EndDisabled();
	ImGui::End();

	// 共有メッシュのグローバル設定はRenderer自身が描画する
	renderer_->DrawImGui();

	AudioManager::GetInstance().DrawImGui();

	// シーン全体の設定はSceneLight自身が描画する
	renderer_->GetLight().DrawImGui();
}

// 全オブジェクトを名前クリックで選べる一覧パネル。選択状態の実体はGizmoControllerの
// インデックスベース管理のまま変えず、「クリックされたオブジェクトのインデックスを引いて
// 選択状態にセットする」GizmoController::SetSelected/SetSelected2Dを呼ぶだけに留める
// （既存のGizmoウィンドウ内Targetコンボと選択状態を共有するため、片方で選んでももう片方に反映される）
// ドラッグ&ドロップで親子付けするときのペイロード種別名（BeginDragDropSource/Targetで対応させる）
static const char* kHierarchyDragDropId = "HIERARCHY_GAMEOBJECT";

// 選択中GameObjectの詳細（名前・コンポーネント一覧・Add Component）を表示するUnity風の
// Inspectorウィンドウ。Hierarchyと同じく専用ウィンドウとして独立させ、「Objects」ウィンドウは
// 生成UIのみを残す。コンポーネント一覧の見出し・並び替え・右クリックメニューはGameObject::
// DrawImGui() → ComponentManager::DrawImGui()に委譲する
void SceneBase::DrawInspector() {
	ImGui::Begin("インスペクター##Inspector");

	GameObject* selected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	if (selected) {
		// 名前編集欄（Unityの一番上のNameフィールド相当）。選択が変わったらバッファを詰め直す
		static char nameBuf[128] = "";
		static GameObject* lastObj = nullptr;
		if (selected != lastObj) {
			strncpy_s(nameBuf, selected->name.c_str(), sizeof(nameBuf) - 1);
			lastObj = selected;
		}
		if (ImGui::InputText("名前", nameBuf, sizeof(nameBuf))) selected->name = nameBuf;
		ImGui::Separator();

		selected->DrawImGui();
		ImGui::Separator();
		DrawAddComponentMenu(*selected);
	} else {
		ImGui::TextDisabled("(オブジェクト未選択)");
	}

	ImGui::End();
}

void SceneBase::DrawHierarchy() {
	ImGui::Begin("ヒエラルキー##Hierarchy");

	GameObject* current = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);

	// 1オブジェクト分のノードを描画し、GetChildren()を再帰的に描画する。
	// 選択はis2Dで実際の所属を見て正しいSetSelected/SetSelected2Dへ振り分ける
	// （どちらのセクションから辿り着いたかに関係なく、子は親と異なるis2Dを持つ可能性があるため）
	std::function<void(GameObject*)> drawNode = [&](GameObject* obj) {
		ImGui::PushID(obj);
		// hasChildrenはこのノードを開く前に1回だけ確定する。ドロップ処理（下のBeginDragDropTarget）
		// でこのノードの子が増減する可能性があり、TreeNodeExへ渡したフラグ（Leaf/NoTreePushOnOpen）と
		// 後段のTreePop要否判定が食い違うとPushID/TreePopの対応が崩れてImGuiがクラッシュするため、
		// 同じ値を最後まで使い回す
		bool hasChildren = !obj->GetChildren().empty();
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (obj == current) flags |= ImGuiTreeNodeFlags_Selected;

		bool opened = ImGui::TreeNodeEx(obj->name.c_str(), flags);
		if (ImGui::IsItemClicked()) {
			bool is2D = obj->GetComponent<TransformComponent>()->is2D;
			if (is2D) gizmoController_.SetSelected2D(obj, screenTargets_);
			else      gizmoController_.SetSelected(obj, gizmoTargets_);
		}

		// ドラッグ元：このノードをつまんで他ノードへドロップすると子になる
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(kHierarchyDragDropId, &obj, sizeof(GameObject*));
			ImGui::Text("%s", obj->name.c_str());
			ImGui::EndDragDropSource();
		}
		// ドロップ先：ドロップされたオブジェクトをこのノードの子にする
		// （自分自身/自分の子孫を親にしようとした場合はSetParent内部で無視される）
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kHierarchyDragDropId)) {
				GameObject* dropped = *static_cast<GameObject**>(payload->Data);
				dropped->SetParent(obj);
				RebuildDerivedLists();
			}
			ImGui::EndDragDropTarget();
		}

		if (opened && hasChildren) {
			// スナップショットを取ってから回す：再帰中の子ノードへのドロップでobj->GetChildren()
			// 自体が増減しても、今回描画中のこのループが範囲外アクセスや無限ループにならないようにする
			std::vector<GameObject*> childrenSnapshot = obj->GetChildren();
			for (GameObject* child : childrenSnapshot) drawNode(child);
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	if (ImGui::SmallButton("+ フォルダ")) {
		CreateObject("フォルダ " + std::to_string(objects_.size() + 1));
		RebuildDerivedLists();
	}
	ImGui::SameLine();
	// UI（2D）用の空オブジェクト。is2Dをtrueにするだけでなく、SpriteRender等の2Dオブジェクト
	// 生成時（DrawAddComponentMenu）と同じpx単位のドラッグ速度・範囲に揃える。揃えないと
	// Inspectorのスライダーが既定の3D用刻み（-10〜10）のままになり、pxで動かす子と感覚が合わない
	if (ImGui::SmallButton("+ UIフォルダ")) {
		GameObject& folder = CreateObject("UIフォルダ " + std::to_string(objects_.size() + 1));
		TransformComponent* t = folder.GetComponent<TransformComponent>();
		t->is2D = true;
		t->translationSpeed = 1.0f; t->translationMin = 0.0f; t->translationMax = 1920.0f;
		t->scaleSpeed = 1.0f; t->scaleMin = 1.0f; t->scaleMax = 1920.0f;
		RebuildDerivedLists();
	}
	ImGui::Separator();

	// 3D/2Dの区分け表示は廃止し、objects_を1回だけなぞってルート（親を持たない）かつ
	// excludeFromGizmoListでないものだけを描画する（除外条件はgizmoTargets_と同じ基準に統一）。
	// gizmoTargets_/screenTargets_の2リストに分けて描画していた頃は、is2Dは独立フィルタで
	// 排他ではないため同じGameObjectが3D/2D両方のセクションに重複表示されることがあったが、
	// objects_を単一ソースとしてなぞることでその重複自体が起きなくなった
	std::vector<GameObject*> snapshot;
	snapshot.reserve(objects_.size());
	for (auto& obj : objects_) snapshot.push_back(obj.get());
	for (GameObject* obj : snapshot) {
		if (obj->GetParent() == nullptr && !obj->excludeFromGizmoList) drawNode(obj);
	}

	// 残りの余白へドロップしたら親子解除（ルートへ戻す）。Unityの「Hierarchyの空きにドラッグ」と同じ操作感。
	// InvisibleButtonは幅・高さのどちらかが0だとIM_ASSERTで落ちるため、リストがウィンドウを
	// ちょうど埋めて余白が0（またはウィンドウが極端に小さい）場合に備えて最低1pxを保証する
	ImVec2 rootDropSize = ImGui::GetContentRegionAvail();
	if (rootDropSize.x < 1.0f) rootDropSize.x = 1.0f;
	if (rootDropSize.y < 1.0f) rootDropSize.y = 1.0f;
	// ドラッグではなくただクリックしただけの場合はtrueが返るので、選択も解除する
	// （Unityの「Hierarchyの空きをクリックすると選択解除」と同じ操作感）
	if (ImGui::InvisibleButton("##hierarchy_root_drop", rootDropSize)) {
		gizmoController_.ResetSelection();
	}
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kHierarchyDragDropId)) {
			GameObject* dropped = *static_cast<GameObject**>(payload->Data);
			dropped->SetParent(nullptr);
			RebuildDerivedLists();
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::End();
}
