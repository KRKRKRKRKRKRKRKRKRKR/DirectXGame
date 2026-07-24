#include "SceneBase.h"
#include "../Externals/imgui/imgui.h"
#include "../Externals/ImGuizmo/src/ImGuizmo.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
#include "../Math/VectorMath.h"
#include "../Math/JsonUtil.h"
#include "../Engine/Audio/AudioManager.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Engine/Utils/StringUtils.h"
#include "../Engine/Utils/Logger.h"
#include "../Engine/Utils/EditorState.h"
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <format>
#include <filesystem>
#include <cctype>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

GameObject& SceneBase::CreateObject(const std::string& name) {
	auto obj = std::make_unique<GameObject>();
	obj->name = name;
	GameObject& ref = *obj;
	objects_.push_back(std::move(obj));
	return ref;
}

GameObject* SceneBase::FindObjectByTag(const std::string& tag) {
	if (tag.empty()) return nullptr; // 空タグ（Untagged）は「タグ無し」の意味なので検索対象にしない
	for (auto& obj : objects_) {
		if (obj->tag == tag) return obj.get();
	}
	return nullptr;
}

GameObject& SceneBase::CreateDynamicTextObject(const std::string& name, const std::string& fontPath, float fontSize,
	TextRenderComponent::TextProvider provider, uint32_t canvasWidth, uint32_t canvasHeight) {
	GameObject& obj = CreateObject(name);
	obj.excludeFromGizmoList = true; // Sprite2Dと同じくスクリーン空間UIなのでGizmo選択対象からは外す
	obj.GetTransform().translation = { 10.0f, 10.0f, 0.0f };

	TextRenderComponent* text = TextRenderComponent::CreateDynamic(obj, renderer_, fontPath, fontSize, canvasWidth, canvasHeight);
	text->hudKey = name; // 呼び出し元は常にhudDefinitions_のキー名をnameとして渡す（CreateHud参照）
	text->SetTextProvider(std::move(provider));

	return obj;
}

void SceneBase::SaveScene(const std::string& saveName) {
	SceneObjectStore::Save(assetFolder_, objects_, saveName);
	// 名前付きスナップショットを保存した直後は一覧に反映しておく（既定保存では一覧は変わらない）
	if (!saveName.empty()) RescanSavedSnapshots();
}

void SceneBase::LoadScene(const std::string& saveName) {
	ComponentLoadContext ctx{ renderer_, &textures_ };
	if (SceneObjectStore::Load(assetFolder_, objects_, ctx, saveName)) {
		RebindDynamicTextProviders();
		RebuildDerivedLists();
		gizmoController_.ResetSelection();
	}
}

void SceneBase::RescanSavedSnapshots() {
	savedSnapshotNames_.clear();
	namespace fs = std::filesystem;
	const std::string prefix = "scene_";
	const std::string suffix = ".json";
	std::error_code ec;
	std::string dir = "Resources/" + assetFolder_;
	if (!fs::exists(dir, ec)) return;
	for (auto& entry : fs::directory_iterator(dir, ec)) {
		if (ec || !entry.is_regular_file()) continue;
		std::string filename = entry.path().filename().string();
		if (filename.size() > prefix.size() + suffix.size()
			&& filename.compare(0, prefix.size(), prefix) == 0
			&& filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
			savedSnapshotNames_.push_back(filename.substr(prefix.size(), filename.size() - prefix.size() - suffix.size()));
		}
	}
	selectedSnapshotIndex_ = 0;
}

void SceneBase::DeleteSelectedObject() {
	// 矩形選択（複数選択）中はそちらを優先して一括削除する。矩形選択していなければ
	// 従来通りGizmoで選択中の1オブジェクトだけを消す
	const std::vector<GameObject*>& multiSelected = gizmoController_.GetMultiSelected2D();
	if (!multiSelected.empty()) {
		DeleteObjects(multiSelected);
		return;
	}
	GameObject* selected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	if (!selected) return;
	DeleteObjects({ selected });
}

void SceneBase::DeleteObjects(const std::vector<GameObject*>& roots) {
	if (roots.empty()) return;

	// 削除対象はrootsに渡されたオブジェクト自身だけでなく、子孫もすべて含める（カスケード削除）。
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
		bool is2D = obj->GetComponent<TransformComponent>()->is2D;
		// is2Dなオブジェクト（Sprite2D/Text等）はTransform.translation/scaleがピクセル座標系のため、
		// 3D側（gizmoTargets_、ワールド空間レイキャストのUpdatePicking）に混ざると、pxのscale値が
		// そのままワールド空間のBounding Sphere半径として使われてしまい、実際の見た目と無関係な
		// 巨大な当たり判定球になる（＝3Dビューでどこをクリックしても常にis2Dオブジェクトが
		// 最優先でヒットしてしまう不具合の原因だった）。is2DはscreenTargets_側だけに入れる
		if (!obj->excludeFromGizmoList && !is2D) gizmoTargets_.push_back(obj.get());
		if (is2D) screenTargets_.push_back(obj.get());
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

	RescanProjectAssets(); // プロジェクトパネル（画像/音声）用にResources/を一度走査しておく
	RescanSavedSnapshots(); // 名前を付けて保存した既存スナップショット一覧を読み込んでおく
}

void SceneBase::RescanProjectAssets() {
	projectImages_.clear();
	projectAudioClips_.clear();
	namespace fs = std::filesystem;
	static const std::vector<std::string> kImageExt = { ".png", ".jpg", ".jpeg", ".bmp", ".dds", ".tga" };
	static const std::vector<std::string> kAudioExt  = { ".wav", ".mp3" };

	std::error_code ec;
	if (!fs::exists("Resources", ec)) return;
	for (auto& entry : fs::recursive_directory_iterator("Resources", ec)) {
		if (ec || !entry.is_regular_file()) continue;
		std::string ext = entry.path().extension().string();
		for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		// generic_string()でパス区切りを"/"に統一する（Windowsの"\"のままだとRenderer::LoadTexture等の
		// 既存コードが前提とする"Resources/xxx"形式の文字列比較・分割と食い違うため）
		std::string path = entry.path().generic_string();
		std::string name = entry.path().filename().string();
		if (std::find(kImageExt.begin(), kImageExt.end(), ext) != kImageExt.end()) {
			projectImages_.push_back({ path, name });
		} else if (std::find(kAudioExt.begin(), kAudioExt.end(), ext) != kAudioExt.end()) {
			projectAudioClips_.push_back({ path, name });
		}
	}
}

std::string SceneBase::EnsureTextureRegistered(const std::string& path) {
	std::string filename = path.substr(path.find_last_of('/') + 1);
	for (auto& t : textures_) {
		if (t.name == filename) return t.name;
	}
	TextureHandle handle = renderer_->LoadTexture(path);
	textures_.push_back({ handle, filename });
	return filename;
}

void SceneBase::AttachTextureAsset(GameObject& obj, const std::string& path) {
	// Cube/Sphere/Sprite Render等の描画コンポーネントが無いと貼り付け先が無いため、
	// Add Componentメニューの「テクスチャ選択」と同じ前提条件をここでも守る
	if (!obj.GetComponent<RenderComponentBase>()) {
		Logger::Log("画像のドロップには先にCube/Sphere/Sprite Render等が必要です\n");
		return;
	}
	std::string texName = EnsureTextureRegistered(path);
	if (obj.GetComponent<TextureSelectorComponent>()) {
		// TextureSelectorComponentは生成後にインデックスを差し替える公開APIが無いため、
		// 既に付いている場合は一旦外してから新しいテクスチャ名で作り直す
		ComponentRegistry::RemoveByTypeName("TextureSelector", obj);
	}
	ComponentLoadContext ctx{ renderer_, &textures_ };
	nlohmann::json data;
	data["textureName"] = texName;
	ComponentRegistry::Create("TextureSelector", obj, ctx, data);
}

void SceneBase::AttachAudioAsset(GameObject& obj, const std::string& path) {
	std::string filename = path.substr(path.find_last_of('/') + 1);
	size_t dot = filename.find_last_of('.');
	std::string stem = (dot == std::string::npos) ? filename : filename.substr(0, dot);

	ComponentLoadContext ctx{ renderer_, &textures_ };
	nlohmann::json data;
	data["filePath"] = path;
	data["registeredName"] = stem;
	data["soundType"] = static_cast<int>(SoundType::SE);
	data["loop"] = false;
	ComponentRegistry::Create("AudioSource", obj, ctx, data);
}

namespace {
bool IsValidScriptBaseName(const std::string& s) {
	if (s.empty()) return false;
	if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
	for (char c : s) {
		if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
	}
	return true;
}

// vcxproj/.vcxproj.filtersへの追記共通処理。insertBeforeMarker（本体は"<Import Project=..."、
// .filtersは"</Project>"）の直前に、className用のClCompile/ClIncludeを1つの新しい<ItemGroup>
// として挿入する。既存の<ItemGroup>を書き換えるのではなく別グループを追加するだけなので、
// XMLパーサ無しの単純なテキスト検索・挿入で安全に行える
bool AppendScriptToProjectFile(const std::string& filePath, const std::string& insertBeforeMarker, const std::string& className) {
	std::ifstream in(filePath, std::ios::binary);
	if (!in.is_open()) return false;
	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();

	size_t pos = content.find(insertBeforeMarker);
	if (pos == std::string::npos) return false;

	std::string block =
		"  <ItemGroup>\r\n"
		"    <ClCompile Include=\"Game\\Scripts\\" + className + ".cpp\" />\r\n"
		"    <ClInclude Include=\"Game\\Scripts\\" + className + ".h\" />\r\n"
		"  </ItemGroup>\r\n";
	content.insert(pos, block);

	std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) return false;
	out << content;
	return true;
}
} // namespace

void SceneBase::CreateNewScript(const std::string& baseName, const std::string& displayName, const std::string& category) {
	if (!IsValidScriptBaseName(baseName)) {
		lastScriptCreationMessage_ = "スクリプト名が不正です（英字/_で始まり、英数字と_のみ使えます）";
		Logger::Log("CreateNewScript: 不正なスクリプト名 '" + baseName + "'\n");
		return;
	}

	// GravityComponent⇔"Gravity"の既存命名規則に合わせる：クラス名は必ず"Component"で終わり、
	// typeNameはその"Component"を除いたものにする
	constexpr const char* kSuffix = "Component";
	bool alreadyHasSuffix = baseName.size() >= 9 && baseName.compare(baseName.size() - 9, 9, kSuffix) == 0;
	std::string className = alreadyHasSuffix ? baseName : baseName + kSuffix;
	std::string typeName = alreadyHasSuffix ? baseName.substr(0, baseName.size() - 9) : baseName;
	if (typeName.empty()) typeName = className; // "Component"だけが渡された等、念のためのフォールバック

	namespace fs = std::filesystem;
	std::string headerPath = "Game/Scripts/" + className + ".h";
	std::string sourcePath = "Game/Scripts/" + className + ".cpp";
	if (fs::exists(headerPath) || fs::exists(sourcePath)) {
		lastScriptCreationMessage_ = className + " は既に存在します（上書きしません）";
		Logger::Log("CreateNewScript: " + className + " は既に存在するため中断\n");
		return;
	}

	std::error_code ec;
	fs::create_directories("Game/Scripts", ec);

	std::string headerContent =
		"#pragma once\r\n"
		"#include \"../../Engine/GameObject/IComponent.h\"\r\n"
		"\r\n"
		"// TODO: このコンポーネントの説明を書く\r\n"
		"class " + className + " : public IComponent {\r\n"
		"public:\r\n"
		"\tvoid Update(float deltaTime, Transform& transform) override;\r\n"
		"\tvoid DrawImGui(const char* namePrefix) override;\r\n"
		"\tvoid ToJson(nlohmann::json& out) const override;\r\n"
		"\tvoid FromJson(const nlohmann::json& in) override;\r\n"
		"};\r\n";

	std::string sourceContent =
		"#include \"" + className + ".h\"\r\n"
		"#include \"../../Engine/GameObject/ComponentRegistry.h\"\r\n"
		"#include \"../../Externals/imgui/imgui.h\"\r\n"
		"\r\n"
		"void " + className + "::Update(float deltaTime, Transform& transform) {\r\n"
		"\t(void)deltaTime; (void)transform;\r\n"
		"\t// TODO: 毎フレームの処理\r\n"
		"}\r\n"
		"\r\n"
		"void " + className + "::DrawImGui(const char* namePrefix) {\r\n"
		"\t(void)namePrefix;\r\n"
		"\t// TODO: Inspectorに表示する項目\r\n"
		"}\r\n"
		"\r\n"
		"void " + className + "::ToJson(nlohmann::json& out) const {\r\n"
		"\t(void)out;\r\n"
		"\t// TODO: 保存するフィールド\r\n"
		"}\r\n"
		"\r\n"
		"void " + className + "::FromJson(const nlohmann::json& in) {\r\n"
		"\t(void)in;\r\n"
		"\t// TODO: 読み込むフィールド\r\n"
		"}\r\n"
		"\r\n"
		"REGISTER_SIMPLE_COMPONENT(" + className + ", \"" + typeName + "\", \"" + displayName + "\", \"" + category + "\");\r\n";

	std::ofstream headerFile(headerPath, std::ios::binary);
	std::ofstream sourceFile(sourcePath, std::ios::binary);
	if (!headerFile.is_open() || !sourceFile.is_open()) {
		lastScriptCreationMessage_ = "ファイルの書き出しに失敗しました";
		Logger::Log("CreateNewScript: " + headerPath + " / " + sourcePath + " の書き出しに失敗\n");
		return;
	}
	headerFile << headerContent;
	sourceFile << sourceContent;
	headerFile.close();
	sourceFile.close();

	bool vcxprojOk = AppendScriptToProjectFile(
		"ClearRootEngine.vcxproj", "<Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />", className);
	bool filtersOk = AppendScriptToProjectFile(
		"ClearRootEngine.vcxproj.filters", "</Project>", className);
	if (!vcxprojOk || !filtersOk) {
		Logger::Log("CreateNewScript: .vcxproj / .vcxproj.filtersへの自動登録に失敗しました（手動で追記してください）\n");
	}

	// 既定の関連付けアプリ（Visual Studio等）でひな形を開く。ShellExecuteはファイルパスの
	// 区切りは"/"のままでも動くが、Windows流儀に合わせて"\"へ変換しておく
	std::string headerPathWin = headerPath, sourcePathWin = sourcePath;
	std::replace(headerPathWin.begin(), headerPathWin.end(), '/', '\\');
	std::replace(sourcePathWin.begin(), sourcePathWin.end(), '/', '\\');
	ShellExecuteA(nullptr, "open", headerPathWin.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	ShellExecuteA(nullptr, "open", sourcePathWin.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

	lastScriptCreationMessage_ = className + " を作成しました。Visual Studioでプロジェクトを再読み込みしてビルドしてください";
	Logger::Log("CreateNewScript: " + className + " を作成し、vcxproj/filtersへ登録しました\n");
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
	// hudKeyでhudDefinitions_を引いて対応するProviderを付け直す。hudKeyが空のまま保存された
	// 古いシーンデータ（hudKeyフィールド追加前）に限りGameObject名をフォールバックに使う
	for (auto& obj : objects_) {
		auto* text = obj->GetComponent<TextRenderComponent>();
		if (!text || !text->dynamicText) continue;
		const std::string& key = !text->hudKey.empty() ? text->hudKey : obj->name;
		for (auto& entry : hudDefinitions_) {
			if (entry.first != key) continue;
			text->SetTextProvider(entry.second.provider);
			break;
		}
	}
}

void SceneBase::Render(float deltaTime) {
	lastDeltaTime_ = deltaTime; // hudDefinitions_内のFPS用providerが参照する直近のフレーム時間

	// ImGui::NewFrame()の後、ImGui::Render()の前に呼ぶ必要がある
	ImGuizmo::BeginFrame();

	// エディタUIが非表示の間（＝Releaseビルドの既定状態、またはDebugでF11を押した後）は
	// 常にPlay状態にする。UIが無いと"再生"ボタンを押す手段が無いままisPlaying_==falseに
	// 固定され、Gravity/AutoRun/PlayerController等のコンポーネントが一切更新されなくなるため
	// （viewingGameCamera_をUI非表示時に強制trueにする下のロジックと同じ考え方）
	if (!EditorState::GetInstance().IsUiVisible()) {
		isPlaying_ = true;
	}

	// isPlaying_中のみコンポーネント更新（Stop中はGizmoで自由に配置できるようにする）
	if (isPlaying_) {
		// CameraFollowComponentのtarget解決：まずタグ"Player"が付いたオブジェクトを優先し、
		// 無ければ（今までどおり）シーン内で最初に見つかったAutoRunComponent持ちオブジェクトに
		// フォールバックする。各オブジェクトのUpdate()より前に解決しておくことで、このフレームの
		// CameraFollowComponent::Updateが古い/nullのtargetを見ないようにする
		GameObject* autoRunTarget = FindObjectByTag("Player");
		if (!autoRunTarget) {
			for (auto& obj : objects_) {
				if (obj->GetComponent<AutoRunComponent>()) { autoRunTarget = obj.get(); break; }
			}
		}
		for (auto& obj : objects_) {
			if (auto* follow = obj->GetComponent<CameraFollowComponent>()) {
				follow->target = autoRunTarget;
			}
		}

		for (auto* obj : gizmoTargets_) obj->Update(deltaTime);
	}

	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetSceneViewportWidth(), renderer_->GetSceneViewportHeight()));

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

	// Gameモード用カメラの候補を探す：まずタグ"MainCamera"が付いたオブジェクトを優先し、
	// 無ければ（今までどおり）シーン内で最初に見つかったCameraComponentにフォールバックする。
	// 複数カメラがある場合にどれを使うか明示的に選べるように、Unityの「MainCameraタグ」相当を追加した
	CameraComponent* gameCamera = nullptr;
	GameObject* gameCameraObject = nullptr;
	if (GameObject* tagged = FindObjectByTag("MainCamera")) {
		if (auto* c = tagged->GetComponent<CameraComponent>()) {
			gameCamera = c;
			gameCameraObject = tagged;
		}
	}
	if (!gameCamera) {
		for (auto& obj : objects_) {
			if (auto* c = obj->GetComponent<CameraComponent>()) {
				gameCamera = c;
				gameCameraObject = obj.get();
				break;
			}
		}
	}
	// カメラが無くなったら（削除された等）強制的にSceneへ戻す
	if (!gameCamera) viewingGameCamera_ = false;
	// エディタUIを隠している間は常にGameカメラ視点を使う（Sceneの自由カメラ編集は無効化する）
	if (!EditorState::GetInstance().IsUiVisible() && gameCamera) viewingGameCamera_ = true;
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
			camera_->GetAspectRatio(renderer_->GetSceneViewportWidth(), renderer_->GetSceneViewportHeight()));
		// positionOffset込みの実際の視点位置（ライティングの鏡面反射計算等で使われる）
		activeCamPos = gameCamera->GetEffectiveWorldTransform(camWorld).translation;
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

	// CameraComponentにはメッシュが無く、選択しても向き・画角・映る範囲が分からないため、
	// Blenderのカメラを模したワイヤーフレームで可視化する（Scene表示中のみ。Game表示中は
	// その視点自体を描いていることになるので不要）
	if (!useGameCamera) {
		float cameraGizmoAspect = camera_->GetAspectRatio(renderer_->GetSceneViewportWidth(), renderer_->GetSceneViewportHeight());
		for (auto& obj : objects_) {
			if (auto* cam = obj->GetComponent<CameraComponent>()) {
				// positionOffset/rotationOffset込みの「実際に見ている位置」にアイコンを描く
				// （オーナーの生のTransformのまま描くと、オフセットで実際の視点とズレて表示される）
				Transform effective = cam->GetEffectiveWorldTransform(obj->GetWorldTransform());
				cam->DrawGizmoVisualization(renderer_, effective, activeView, activeProj, cameraGizmoAspect);
			}
		}
	}

	if (EditorState::GetInstance().IsUiVisible()) {
		DrawImGui();
	}

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
		camera_->GetAspectRatio(renderer_->GetSceneViewportWidth(), renderer_->GetSceneViewportHeight()));
	renderer_->DrawGridBatch(viewMatrix, projMatrix);
}

namespace {
struct ComponentMenuEntry { std::string displayName; std::string typeName; };
struct ComponentMenuCategory { std::string label; std::vector<ComponentMenuEntry> items; };

// 既定値（空JSON）のまま安全に生成できるRegisterSimple系コンポーネントを、カテゴリごとに
// グルーピングして返す。以前はここに手書きのテーブルを持っていたが、各コンポーネントが
// REGISTER_SIMPLE_COMPONENTマクロで自己登録するようになったため、ComponentRegistryが
// 実際に知っている内容（GetSimpleTypeNames()の登録順＋GetCategory()）からその場で組み立てる。
// 新しいSimpleコンポーネントを追加しても、このテーブルには一切手を入れる必要が無い
// （唯一の登録行はコンポーネント自身の.cpp末尾のREGISTER_SIMPLE_COMPONENTだけになる）。
// SceneBase::DrawAddComponentMenu（クリックで即追加のポップアップ）が参照する
const std::vector<ComponentMenuCategory>& GetInstantAddCategories() {
	static const std::vector<ComponentMenuCategory> categories = [] {
		std::vector<ComponentMenuCategory> result;
		for (const std::string& typeName : ComponentRegistry::GetSimpleTypeNames()) {
			std::string category = ComponentRegistry::GetCategory(typeName);
			std::string displayName = ComponentRegistry::GetDisplayName(typeName);
			auto it = std::find_if(result.begin(), result.end(),
				[&](const ComponentMenuCategory& c) { return c.label == category; });
			if (it == result.end()) {
				result.push_back({ category, { { displayName, typeName } } });
			} else {
				it->items.push_back({ displayName, typeName });
			}
		}
		return result;
	}();
	return categories;
}
} // namespace

// Unity風：「+ コンポーネントを追加」ボタン1つだけを常設し、押したときだけポップアップで
// カテゴリ分けした一覧（形状/物理/ライティング/カメラ/描画/オーディオ）を出す。以前はここに
// 全カテゴリを常時展開していたためInspectorが縦に長くごちゃついていた。削除はGameObject::
// DrawImGui()側（コンポーネント見出しの右クリック→「コンポーネントを削除」、ComponentManager::
// DrawImGui参照）に既に一本化されているため、ここでは追加のみを扱う
void SceneBase::DrawAddComponentMenu(GameObject& selected) {
	if (ImGui::Button("+ コンポーネントを追加")) {
		ImGui::OpenPopup("AddComponentPopup");
	}
	if (!ImGui::BeginPopup("AddComponentPopup")) return;

	ComponentLoadContext ctx{ renderer_, &textures_ };

	// 既定値のまま追加してよい型は、選んだ瞬間に追加してポップアップを閉じる
	auto instantAddCategory = [&](const std::string& categoryLabel, const std::vector<ComponentMenuEntry>& items) {
		ImGui::SeparatorText(categoryLabel.c_str());
		for (const auto& item : items) {
			if (ImGui::Selectable(item.displayName.c_str())) {
				ComponentRegistry::Create(item.typeName, selected, ctx, nlohmann::json::object());
				ImGui::CloseCurrentPopup();
			}
		}
	};

	for (const auto& category : GetInstantAddCategories()) {
		instantAddCategory(category.label, category.items);
	}

	// ---- 描画：コンストラクタ引数や兄弟コンポーネントへの依存があるため個別UIのまま残す ----
	ImGui::SeparatorText("描画");

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
			ImGui::CloseCurrentPopup();
		}
		if (!canAdd) ImGui::EndDisabled();
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
			ImGui::CloseCurrentPopup();
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
		} else if (ImGui::Button("追加##TextureSelector")) {
			nlohmann::json data;
			data["textureName"] = textures_[0].name;
			ComponentRegistry::Create("TextureSelector", selected, ctx, data);
			ImGui::CloseCurrentPopup();
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
			ImGui::CloseCurrentPopup();
		}
		ImGui::TreePop();
	}

	// TextRender：HUD（動的、hudDefinitions_のテンプレートから選ぶ）と静的テキスト
	// （内容を打ち込む）の2種類をここから選択中オブジェクトへ直接アタッチする
	// （旧ヒエラルキー「HUD/テキストを作成」は専用の新規オブジェクトを作る方式だったが、
	// こちらは他の描画コンポーネントと同じくAdd Componentから選択中オブジェクトへ付与する）
	if (ImGui::TreeNode("テキスト描画")) {
		static bool isDynamicHud = true;
		ImGui::Checkbox("HUD（動的）", &isDynamicHud);
		if (isDynamicHud) {
			static int hudIndex = 0;
			// コンボの表示だけ日本語にする。TextRenderComponent::hudKeyに書き込むのは
			// entry.first（英語キー）のほう（RebindDynamicTextProvidersの照合に使うため）
			std::vector<std::string> hudDisplayNames;
			for (auto& entry : hudDefinitions_) {
				hudDisplayNames.push_back(entry.first == "Camera Coord" ? "カメラ座標" : entry.first);
			}
			std::vector<const char*> hudNamesRaw;
			for (auto& n : hudDisplayNames) hudNamesRaw.push_back(n.c_str());
			if (!hudNamesRaw.empty()) {
				if (hudIndex >= static_cast<int>(hudNamesRaw.size())) hudIndex = 0;
				ImGui::Combo("HUD種別", &hudIndex, hudNamesRaw.data(), static_cast<int>(hudNamesRaw.size()));
				if (ImGui::Button("追加##TextRenderHud")) {
					const auto& [hudName, def] = hudDefinitions_[hudIndex];
					TextRenderComponent* text = TextRenderComponent::CreateDynamic(
						selected, renderer_, "Resources/Font/font.ttf", 20.0f, def.canvasWidth, def.canvasHeight);
					text->hudKey = hudName;
					text->SetTextProvider(def.provider);
					RebuildDerivedLists(); // is2DがtrueになったのでscreenTargets_に反映させる（さもないとHierarchyから選択できなくなる）
					ImGui::CloseCurrentPopup();
				}
			}
		} else {
			// 静的テキストはGameObjectの名前をそのままファイル名に使う（Resources/{assetFolder_}/
			// Text/{name}.txt）。ImGuiのInputTextからのUTF-8文字列をそのままstd::ofstream(std::string)に
			// 渡すと実行時ロケール（日本語環境ならShift-JIS）でパスが解釈され文字化けするため、
			// wstring版に変換してから渡す。"/"・"\"・":"はディレクトリ脱出防止のため取り除く
			static char staticTextContentBuf[1024] = "";
			ImGui::InputTextMultiline("内容", staticTextContentBuf, sizeof(staticTextContentBuf), ImVec2(0, 80));
			bool canAdd = staticTextContentBuf[0] != '\0';
			if (!canAdd) ImGui::BeginDisabled();
			if (ImGui::Button("追加##TextRenderStatic")) {
				std::string sanitizedName = selected.name;
				sanitizedName.erase(std::remove_if(sanitizedName.begin(), sanitizedName.end(),
					[](char c) { return c == '/' || c == '\\' || c == ':'; }), sanitizedName.end());
				std::string txtPath = "Resources/" + assetFolder_ + "/Text/" + sanitizedName + ".txt";
				std::ofstream file(StringUtils::ConvertString(txtPath), std::ios::binary);
				file << staticTextContentBuf;
				file.close();
				TextRenderComponent::CreateStatic(selected, renderer_, txtPath, "Resources/Font/font.ttf", 32.0f);
				staticTextContentBuf[0] = '\0';
				RebuildDerivedLists(); // is2DがtrueになったのでscreenTargets_に反映させる（さもないとHierarchyから選択できなくなる）
				ImGui::CloseCurrentPopup();
			}
			if (!canAdd) ImGui::EndDisabled();
		}
		ImGui::TreePop();
	}

	// ---- オーディオ ----
	ImGui::SeparatorText("オーディオ");

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
			ImGui::CloseCurrentPopup();
		}
		if (!canAdd) ImGui::EndDisabled();
		ImGui::TreePop();
	}

	ImGui::EndPopup();
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
	// UI（is2D）とObject（3D）をResources/{assetFolder_}/ui.json・scene.jsonの2ファイルに分けて保存/復元する。
	// 引数なしのSaveScene()/LoadScene()は常にこの既定ファイルを指す（起動時の自動ロードもこれ）
	if (ImGui::Button("保存")) SaveScene();
	ImGui::SameLine();
	if (ImGui::Button("読み込み")) LoadScene();

	// 名前を付けて保存：既定のscene.json/ui.jsonとは別に、名前付きスナップショットを追加保存する
	// （テスト配置を複数残しておきたい等の用途。既定の保存/読み込みは上のボタンのまま変わらない）
	ImGui::SameLine();
	if (ImGui::Button("名前を付けて保存")) ImGui::OpenPopup("SaveAsPopup");
	if (ImGui::BeginPopup("SaveAsPopup")) {
		static char saveAsNameBuf[128] = "";
		ImGui::InputText("保存名", saveAsNameBuf, sizeof(saveAsNameBuf));
		bool canSave = saveAsNameBuf[0] != '\0';
		if (!canSave) ImGui::BeginDisabled();
		if (ImGui::Button("保存##SaveAsConfirm")) {
			// ファイル名に使えない文字を除去する（既存の静的テキスト保存箇所と同じサニタイズ）
			std::string sanitized = saveAsNameBuf;
			sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
				[](char c) { return c == '/' || c == '\\' || c == ':'; }), sanitized.end());
			if (!sanitized.empty()) {
				SaveScene(sanitized);
				saveAsNameBuf[0] = '\0';
				ImGui::CloseCurrentPopup();
			}
		}
		if (!canSave) ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("キャンセル##SaveAsCancel")) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (!savedSnapshotNames_.empty()) {
		int current = std::clamp(selectedSnapshotIndex_, 0, static_cast<int>(savedSnapshotNames_.size()) - 1);
		if (ImGui::BeginCombo("保存済みスナップショット", savedSnapshotNames_[current].c_str())) {
			for (int i = 0; i < static_cast<int>(savedSnapshotNames_.size()); i++) {
				bool selected = (i == current);
				if (ImGui::Selectable(savedSnapshotNames_[i].c_str(), selected)) selectedSnapshotIndex_ = i;
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("選択したスナップショットを読み込み")) LoadScene(savedSnapshotNames_[selectedSnapshotIndex_]);
	}

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
	DrawProjectPanel();

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

// プロジェクトパネル→ヒエラルキー/インスペクターへのドラッグ&ドロップ用ペイロードID（3種）。
// スクリプトはGetInstantAddCategories()の静的テーブル内のstd::string、画像/音声はSceneBaseの
// メンバprojectImages_/projectAudioClips_内のstd::stringを指すポインタを運ぶ
// （テーブル/メンバとも、ドラッグ中に再構築されない限りアドレスが安定しているため）
static const char* kProjectComponentDragDropId = "PROJECT_COMPONENT_TYPE";
static const char* kProjectImageDragDropId     = "PROJECT_IMAGE_ASSET";
static const char* kProjectAudioDragDropId     = "PROJECT_AUDIO_ASSET";

namespace {
// ComponentManager.cpp内のTypeNameToColor()と同じアルゴリズム（意図的な複製）。typeNameの
// 文字列ハッシュから色を決めるので、同じ型は常に同じ色のアイコンになる
ImVec4 ComponentIconColor(const std::string& typeName) {
	uint32_t hash = 2166136261u; // FNV-1a
	for (char ch : typeName) {
		hash ^= static_cast<uint8_t>(ch);
		hash *= 16777619u;
	}
	float hue = static_cast<float>(hash % 360) / 360.0f;
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 0.85f, r, g, b);
	return ImVec4(r, g, b, 1.0f);
}
} // namespace

// 選択中GameObjectの詳細（名前・コンポーネント一覧・Add Component）を表示するUnity風の
// Inspectorウィンドウ。空のGameObject自体の新規生成はHierarchyパネルの「+ 新規追加」のみとし、
// 形状（Cube/Sphere/Triangle）・HUD/テキスト等の中身はすべてAdd Componentメニューに一本化した。
// コンポーネント一覧の見出し・並び替え・右クリックメニューはGameObject::
// DrawImGui() → ComponentManager::DrawImGui()に委譲する
void SceneBase::DrawInspector() {
	ImGui::Begin("インスペクター##Inspector");

	GameObject* selected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	if (selected) {
		// 名前・タグ編集欄（Unityの一番上のName/Tagフィールド相当）。選択が変わったらバッファを詰め直す
		static char nameBuf[128] = "";
		static char tagBuf[128] = "";
		static GameObject* lastObj = nullptr;
		if (selected != lastObj) {
			strncpy_s(nameBuf, selected->name.c_str(), sizeof(nameBuf) - 1);
			strncpy_s(tagBuf, selected->tag.c_str(), sizeof(tagBuf) - 1);
			lastObj = selected;
		}
		if (ImGui::InputText("名前", nameBuf, sizeof(nameBuf))) selected->name = nameBuf;
		// MainCamera/PlayerはそれぞれGameビューのカメラ選出・AutoRunComponentのプレイヤー判定で
		// 特別扱いされる（SceneBase::Render参照）。他は自由な文字列でよい
		if (ImGui::InputTextWithHint("タグ", "(例: MainCamera, Player)", tagBuf, sizeof(tagBuf))) selected->tag = tagBuf;
		ImGui::Separator();

		selected->DrawImGui();
		ImGui::Separator();
		DrawAddComponentMenu(*selected);
	} else {
		ImGui::TextDisabled("(オブジェクト未選択)");
	}

	// プロジェクトパネルからのドロップ受け入れ：残りの余白へ落とすと選択中オブジェクトへ
	// スクリプト/画像/音声を付与する。BeginDragDropTargetは直前に何か「アイテム」が要るため、
	// ヒエラルキーの余白ドロップ（##hierarchy_root_drop）と同じくInvisibleButtonを土台にする
	// （幅・高さのどちらかが0だとIM_ASSERTで落ちる既知のクラッシュパターンのため最低1pxを保証）
	ImVec2 dropSize = ImGui::GetContentRegionAvail();
	if (dropSize.x < 1.0f) dropSize.x = 1.0f;
	if (dropSize.y < 1.0f) dropSize.y = 1.0f;
	ImGui::InvisibleButton("##inspector_asset_drop", dropSize);
	if (ImGui::BeginDragDropTarget()) {
		if (selected) {
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kProjectComponentDragDropId)) {
				const std::string& typeName = *(*static_cast<const std::string* const*>(p->Data));
				ComponentLoadContext ctx{ renderer_, &textures_ };
				ComponentRegistry::Create(typeName, *selected, ctx, nlohmann::json::object());
			}
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kProjectImageDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(p->Data));
				AttachTextureAsset(*selected, path);
			}
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kProjectAudioDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(p->Data));
				AttachAudioAsset(*selected, path);
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::End();
}

// Unityの「Projectビュー」相当。ユーザーが追加したスクリプト（登録済みコンポーネント）・
// 画像・音声をカテゴリごとのアイコングリッドとして表示する。アイコンをドラッグして
// ヒエラルキーのオブジェクト行、またはインスペクターへドロップすると実際に付与される
void SceneBase::DrawProjectPanel() {
	ImGui::Begin("プロジェクト##Project");

	// 「+ 新規スクリプト」：ひな形.h/.cppの生成からvcxproj登録・エディタで開くまでを自動化する
	// （CreateNewScript参照）。押した瞬間ではなく名前入力ポップアップを経由する
	if (ImGui::Button("+ 新規スクリプト")) {
		ImGui::OpenPopup("NewScriptPopup");
	}
	ImGui::SameLine();
	if (ImGui::Button("更新")) RescanProjectAssets();

	if (ImGui::BeginPopup("NewScriptPopup")) {
		static char nameBuf[128] = "";
		static char displayNameBuf[128] = "";
		static int categoryIndex = 0;
		const char* categories[] = { "形状", "物理", "ライティング", "カメラ", "その他" };
		ImGui::InputText("スクリプト名", nameBuf, sizeof(nameBuf));
		ImGui::InputTextWithHint("表示名", "(未入力ならスクリプト名と同じ)", displayNameBuf, sizeof(displayNameBuf));
		ImGui::Combo("カテゴリ", &categoryIndex, categories, IM_ARRAYSIZE(categories));
		bool canCreate = nameBuf[0] != '\0';
		if (!canCreate) ImGui::BeginDisabled();
		if (ImGui::Button("作成")) {
			std::string display = displayNameBuf[0] != '\0' ? std::string(displayNameBuf) : std::string(nameBuf);
			CreateNewScript(nameBuf, display, categories[categoryIndex]);
			nameBuf[0] = '\0';
			displayNameBuf[0] = '\0';
			ImGui::CloseCurrentPopup();
		}
		if (!canCreate) ImGui::EndDisabled();
		ImGui::EndPopup();
	}

	if (!lastScriptCreationMessage_.empty()) {
		ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", lastScriptCreationMessage_.c_str());
	}

	ImGui::TextWrapped("ドラッグしてヒエラルキーのオブジェクトまたはインスペクターへドロップすると付与されます");
	ImGui::Separator();

	constexpr float kIconSize = 56.0f;
	constexpr float kTileWidth = 76.0f;
	constexpr ImGuiColorEditFlags kIconFlags =
		ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoDragDrop;
	int columnCount = (std::max)(1, static_cast<int>(ImGui::GetContentRegionAvail().x / kTileWidth));

	// ---- スクリプト：既定値のまま追加できる登録済みコンポーネントを、カテゴリの見出しは
	// 出さずフラットに並べる（画像/音声と並ぶ1カテゴリという位置づけのため） ----
	ImGui::SeparatorText("スクリプト");
	int col = 0;
	for (const auto& category : GetInstantAddCategories()) {
		for (const auto& item : category.items) {
			ImGui::PushID(item.typeName.c_str());
			ImGui::BeginGroup();
			ImVec4 color = ComponentIconColor(item.typeName);
			ImGui::ColorButton("##icon", color, kIconFlags, ImVec2(kIconSize, kIconSize));
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTileWidth - 4.0f);
			ImGui::TextWrapped("%s", item.displayName.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();
			// BeginGroup/EndGroup直後の「最後のアイテム」はTextWrapped（ID無し）のため、
			// 何もフラグを渡さずBeginDragDropSourceを呼ぶとIM_ASSERT(0)で落ちる
			// （ComponentManager.cppの「::」ドラッグハンドルで踏んだのと同じ既知のクラッシュパターン）。
			// ImGuiDragDropFlags_SourceAllowNullIDでID無し要素からのドラッグを明示的に許可する
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				const std::string* typeNamePtr = &item.typeName;
				ImGui::SetDragDropPayload(kProjectComponentDragDropId, &typeNamePtr, sizeof(const std::string*));
				ImGui::Text("%s", item.displayName.c_str());
				ImGui::EndDragDropSource();
			}
			ImGui::PopID();
			col++;
			if (col < columnCount) ImGui::SameLine(0.0f, 12.0f);
			else col = 0;
		}
	}

	// ---- 画像：Resources/配下から見つかった画像ファイルを実テクスチャのサムネイルで表示する ----
	ImGui::SeparatorText("画像");
	col = 0;
	for (const auto& asset : projectImages_) {
		ImGui::PushID(asset.path.c_str());
		ImGui::BeginGroup();
		// LoadTextureは同じパスを二重ロードしない（TextureManagerがパスでキャッシュする）ため、
		// 毎フレーム呼んでもコストは無視できる
		TextureHandle handle = renderer_->LoadTexture(asset.path);
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = renderer_->GetTextureSrvGpuHandle(handle);
		ImGui::Image((ImTextureID)gpu.ptr, ImVec2(kIconSize, kIconSize));
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTileWidth - 4.0f);
		ImGui::TextWrapped("%s", asset.displayName.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndGroup();
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const std::string* pathPtr = &asset.path;
			ImGui::SetDragDropPayload(kProjectImageDragDropId, &pathPtr, sizeof(const std::string*));
			ImGui::Text("%s", asset.displayName.c_str());
			ImGui::EndDragDropSource();
		}
		ImGui::PopID();
		col++;
		if (col < columnCount) ImGui::SameLine(0.0f, 12.0f);
		else col = 0;
	}

	// ---- 音声：波形サムネイル等は無いため、種別を示す固定色のアイコンにする ----
	ImGui::SeparatorText("音声");
	col = 0;
	for (const auto& asset : projectAudioClips_) {
		ImGui::PushID(asset.path.c_str());
		ImGui::BeginGroup();
		ImGui::ColorButton("##icon", ImVec4(0.35f, 0.55f, 0.9f, 1.0f), kIconFlags, ImVec2(kIconSize, kIconSize));
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTileWidth - 4.0f);
		ImGui::TextWrapped("%s", asset.displayName.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndGroup();
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const std::string* pathPtr = &asset.path;
			ImGui::SetDragDropPayload(kProjectAudioDragDropId, &pathPtr, sizeof(const std::string*));
			ImGui::Text("%s", asset.displayName.c_str());
			ImGui::EndDragDropSource();
		}
		ImGui::PopID();
		col++;
		if (col < columnCount) ImGui::SameLine(0.0f, 12.0f);
		else col = 0;
	}

	ImGui::End();
}

void SceneBase::ReorderRootObject(GameObject* dropped, size_t visibleIndex) {
	dropped->SetParent(nullptr); // ルートへ（既にルートなら何もしない）

	size_t from = SIZE_MAX;
	for (size_t i = 0; i < objects_.size(); i++) {
		if (objects_[i].get() == dropped) { from = i; break; }
	}
	if (from == SIZE_MAX) return;

	auto isRootVisible = [](const GameObject* o) { return o->GetParent() == nullptr && !o->excludeFromGizmoList; };

	// objects_内で「dropped自身を除いた」ルート表示対象を先頭から数え、visibleIndex番目の要素の
	// 直前（objects_内での絶対位置）を求める。見つからなければ（=末尾扱い）objects_.size()のままにする
	size_t insertAt = objects_.size();
	size_t count = 0;
	for (size_t i = 0; i < objects_.size(); i++) {
		if (i == from) continue; // 自分自身は数えない
		if (!isRootVisible(objects_[i].get())) continue;
		if (count == visibleIndex) { insertAt = i; break; }
		count++;
	}

	size_t to = insertAt;
	if (to > from) to--; // fromを消すと、それより後ろの目標位置は1つ前へ詰まるため補正
	size_t maxTo = objects_.size() - 1; // eraseの前に、eraseした後の有効な最大挿入位置を求めておく
	if (to > maxTo) to = maxTo;
	if (from == to) return;

	auto moved = std::move(objects_[from]);
	objects_.erase(objects_.begin() + from);
	objects_.insert(objects_.begin() + to, std::move(moved));
}

void SceneBase::DrawHierarchy() {
	ImGui::Begin("ヒエラルキー##Hierarchy");

	GameObject* current = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);

	// 右クリックメニューで「削除」が押された対象。drawNode（再帰中）でその場でobjects_.erase()すると、
	// 削除したobjを使い続けている呼び出し元スタック（hasChildren判定・子の再帰描画・TreePop等）が
	// 解放済みポインタに触れてしまうため、木構造の描画が全部終わってから実際に削除する
	GameObject* pendingHierarchyDelete = nullptr;

	// 兄弟同士の「境目」に挟む薄い透明なドロップターゲット。ここへドロップすると、
	// onDropが指定した位置への挿入（並べ替え・別の親への移動）になる。ノード本体へのドロップ
	// （drawNode内の既存のBeginDragDropTarget）は「子として末尾に追加」のまま変更しない
	auto drawInsertionGap = [&](const std::function<void(GameObject*)>& onDrop) {
		// GetContentRegionAvail().xは深い階層のインデントやウィンドウ幅次第で0以下になり得る。
		// InvisibleButtonはx/yどちらかが0だとIM_ASSERT(size_arg.x != 0.0f && size_arg.y != 0.0f)で
		// 落ちる（Hierarchyの余白ドロップボタンで踏んだのと同じ既知のクラッシュパターン）ため、
		// 必ず最低1pxを保証してから渡す
		ImVec2 size(ImGui::GetContentRegionAvail().x, 6.0f);
		if (size.x < 1.0f) size.x = 1.0f;
		ImGui::InvisibleButton("##gap", size);
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kHierarchyDragDropId)) {
				GameObject* dropped = *static_cast<GameObject**>(payload->Data);
				onDrop(dropped);
				RebuildDerivedLists();
			}
			ImGui::EndDragDropTarget();
		}
	};

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

		// 右クリックでこのノード専用の削除メニューをマウスカーソル付近に出す（Unityの
		// 「Hierarchyで右クリック→Delete」相当）。BeginPopupContextItemは直前のアイテム
		// （このTreeNodeEx）に紐付き、右クリックで自動的にカーソル位置へ開く
		if (ImGui::BeginPopupContextItem("HierarchyNodeContext")) {
			// 右クリックした時点でこのオブジェクトを選択状態にし、操作対象をInspector等でも明確にする
			bool is2D = obj->GetComponent<TransformComponent>()->is2D;
			if (is2D) gizmoController_.SetSelected2D(obj, screenTargets_);
			else      gizmoController_.SetSelected(obj, gizmoTargets_);

			if (ImGui::MenuItem("削除")) {
				pendingHierarchyDelete = obj;
			}
			ImGui::EndPopup();
		}

		// ドラッグ元：このノードをつまんで他ノードへドロップすると子になる
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(kHierarchyDragDropId, &obj, sizeof(GameObject*));
			ImGui::Text("%s", obj->name.c_str());
			ImGui::EndDragDropSource();
		}
		// ドロップ先：ドロップされたオブジェクトをこのノードの子（末尾）にする
		// （自分自身/自分の子孫を親にしようとした場合はSetParent内部で無視される）
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kHierarchyDragDropId)) {
				GameObject* dropped = *static_cast<GameObject**>(payload->Data);
				dropped->SetParent(obj);
				RebuildDerivedLists();
			}
			// プロジェクトパネルからのドロップ：このオブジェクトへスクリプト/画像/音声を付与する
			// （Unityの「アセットをHierarchyのオブジェクトへドラッグ」に相当）
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectComponentDragDropId)) {
				const std::string& typeName = *(*static_cast<const std::string* const*>(payload->Data));
				ComponentLoadContext ctx{ renderer_, &textures_ };
				ComponentRegistry::Create(typeName, *obj, ctx, nlohmann::json::object());
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectImageDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(payload->Data));
				AttachTextureAsset(*obj, path);
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAudioDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(payload->Data));
				AttachAudioAsset(*obj, path);
			}
			ImGui::EndDragDropTarget();
		}

		if (opened && hasChildren) {
			// スナップショットを取ってから回す：再帰中の子ノードへのドロップでobj->GetChildren()
			// 自体が増減しても、今回描画中のこのループが範囲外アクセスや無限ループにならないようにする
			std::vector<GameObject*> childrenSnapshot = obj->GetChildren();
			for (size_t i = 0; i < childrenSnapshot.size(); i++) {
				// 各子の直前に「ここへ挿入」ゾーンを置く。ドロップされたらi番目の位置へ並べ替える
				ImGui::PushID(static_cast<int>(i));
				drawInsertionGap([&, i](GameObject* dropped) { obj->ReparentAt(dropped, i); });
				ImGui::PopID();
				drawNode(childrenSnapshot[i]);
			}
			// 子リストの末尾にも1つ置く（末尾へ挿入するため）
			ImGui::PushID(static_cast<int>(childrenSnapshot.size()));
			drawInsertionGap([&, count = childrenSnapshot.size()](GameObject* dropped) { obj->ReparentAt(dropped, count); });
			ImGui::PopID();
			ImGui::TreePop();
		}
		ImGui::PopID();
	};

	if (ImGui::SmallButton("+ 新規追加")) {
		CreateObject("新規追加 " + std::to_string(objects_.size() + 1));
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
	size_t rootVisibleIdx = 0;
	for (GameObject* obj : snapshot) {
		if (obj->GetParent() != nullptr || obj->excludeFromGizmoList) continue;
		// 各ルート直下ノードの直前に「ここへ挿入」ゾーンを置く。ドロップされたら
		// ルート表示順でrootVisibleIdx番目の位置へ並べ替える
		ImGui::PushID(static_cast<int>(rootVisibleIdx));
		ImGui::PushID("RootGap");
		drawInsertionGap([&, rootVisibleIdx](GameObject* dropped) { ReorderRootObject(dropped, rootVisibleIdx); });
		ImGui::PopID();
		ImGui::PopID();
		drawNode(obj);
		rootVisibleIdx++;
	}
	// ルート直下リストの末尾にも1つ置く（末尾へ挿入するため）
	ImGui::PushID(static_cast<int>(rootVisibleIdx));
	ImGui::PushID("RootGap");
	drawInsertionGap([&, rootVisibleIdx](GameObject* dropped) { ReorderRootObject(dropped, rootVisibleIdx); });
	ImGui::PopID();
	ImGui::PopID();

	// 残りの余白へドロップしたらルート末尾へ移動する（Unityの「Hierarchyの空きにドラッグ」と同じ操作感）。
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
			ReorderRootObject(dropped, rootVisibleIdx); // ルート末尾へ（ギャップ末尾ドロップと同じ挙動）
			RebuildDerivedLists();
		}
		ImGui::EndDragDropTarget();
	}

	// 右クリックメニューで「削除」が押されていれば、木構造の描画が全部終わった今ここで実際に消す
	if (pendingHierarchyDelete) {
		DeleteObjects({ pendingHierarchyDelete });
	}

	ImGui::End();
}
