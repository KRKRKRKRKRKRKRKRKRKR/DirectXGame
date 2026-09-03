#include "SceneBase.h"
#include "GameTags.h"
#include "FadeManager.h"
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
#include "../Engine/Utils/EditorState.h"
#include "../Engine/GameObject/Systems/HitEffect.h"
#include "../Engine/GameObject/Component/Physics/SpawnMoveComponent.h"
#include <cmath>
#include <algorithm>
#include <cctype>
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

SceneBase::ButtonInteractionResult SceneBase::UpdateButtonAndReflectHover(const char* hitboxTag, const char* textTag) {
	ButtonInteractionResult result;

	GameObject* hitbox = FindObjectByTag(hitboxTag);
	if (!hitbox) return result;
	auto* playButton = hitbox->GetComponent<PlayButtonComponent>();
	if (!playButton) return result;

	result.hovering = playButton->IsHovering();

	if (textTag) {
		if (GameObject* textObj = FindObjectByTag(textTag)) {
			if (auto* text = textObj->GetComponent<AlphabetTextComponent>()) {
				text->displayScaleMultiplier = result.hovering ? playButton->hoverScaleMultiplier : playButton->normalScaleMultiplier;
				text->displayColor = result.hovering ? playButton->hoverColor : playButton->normalColor;
			}
		}
	}

	result.clicked = playButton->ConsumeClicked();
	return result;
}

void SceneBase::SaveScene(const std::string& saveName) {
	SceneObjectStore::Save(assetFolder_, objects_, saveName);
	// 名前付きスナップショットを保存した直後は一覧に反映しておく（既定保存では一覧は変わらない）
	if (!saveName.empty()) RescanSavedSnapshots();
}

void SceneBase::LoadScene(const std::string& saveName) {
	ComponentLoadContext ctx = MakeComponentLoadContext();
	if (SceneObjectStore::Load(assetFolder_, objects_, ctx, saveName)) {
		// AlphabetTextComponentが生成する文字の子GameObject（RebuildAlphabetTextChildren参照）は
		// excludeFromSave=trueのため、ここでロードされた直後は元々存在しない。ただしcomp自身の
		// lastBuiltTextはtext（今回表示したい文字列）と同じ値で保存されている可能性があるため、
		// 明示的に空へ戻して次フレームのUpdateAlphabetTextComponentsが必ず子を作り直すようにする
		for (auto& obj : objects_) {
			if (auto* comp = obj->GetComponent<AlphabetTextComponent>()) {
				comp->lastBuiltText.clear();
				comp->lastBuiltCharScale = -1.0f;
				comp->lastBuiltCharSpacing = -1.0f;
			}
			// DashedLineComponentの子GameObject（RebuildDashedLineSegments参照）も同じ理由
			// （excludeFromSave=trueのためロード直後は存在しない）で、次フレームの
			// UpdateDashedLineComponentsが必ず子を作り直すようにlastBuilt*を不一致値へ戻す
			if (auto* dashedLine = obj->GetComponent<DashedLineComponent>()) {
				dashedLine->lastBuiltDashCount = -1;
				dashedLine->lastBuiltDashWidth = -1.0f;
			}
			// TextGroupComponentの子GameObject（RebuildTextGroupChildren参照）も同じ理由で
			// builtOnceをfalseへ戻し、次フレームのUpdateTextGroupComponentsが必ず子を作り直すようにする
			if (auto* textGroup = obj->GetComponent<TextGroupComponent>()) {
				textGroup->builtOnce = false;
			}
		}

		// ComboPopupComponentが生成するポップアップもexcludeFromSave=trueのため、ここでロードされた
		// 直後は元々存在しない（SpawnComboPopup参照）。activePopup_自体もFromJsonで復元しない設計
		// のため、特別な後始末は不要

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
	nextScene_.clear();

	// テクスチャ一覧を構築する（"なし"は白テクスチャ、それ以外はRescanProjectAssets()が
	// Resources/配下の画像を全部スキャンして登録する）。以前はここで数枚だけハードコードした
	// リストを作っていたが、そのリストに無い画像をTextureSelector/ModelRenderComponentの
	// Inspectorから選ぶと、選択中は動いても保存→再読み込み後に名前解決できず選択が
	// 消えてしまうバグがあった（textures_に無い名前はFromJsonで見つからずindex=-1に戻るため）。
	// Resources/配下の画像を漏れなく登録することで、選んだものが必ず再読み込みできるようにする
	textures_.push_back({ kTextureNone, "なし" });
	// LoadScene()より前に呼ぶ必要がある（TextureSelector/ModelRenderComponentのFromJsonが
	// テクスチャ名→index解決にtextures_を使うため）。projectImages_/projectAudioClips_/
	// projectModels_（プロジェクトパネル用の一覧）の構築もここで済ませる
	RescanProjectAssets();

	OnInitialize(); // シーン固有の初期化はここで行う

	// Resources/{assetFolder_}/ui.json・scene.jsonが既に存在するなら、起動直後に自動で読み込む。
	// LoadScene()はファイルが無ければ何もせず終わる（SceneSerializer::Loadがfalseを返すだけ）ため、
	// 初回起動（保存済みファイルがまだ無い）でも安全に呼べる。存在する場合はOnInitialize()で
	// 作った内容（Camera Coord等）もLoadの結果で上書きされる
	LoadScene();

	RebuildDerivedLists();

	RescanSavedSnapshots(); // 名前を付けて保存した既存スナップショット一覧を読み込んでおく
}

void SceneBase::RescanProjectAssets() {
	projectImages_.clear();
	projectAudioClips_.clear();
	projectModels_.clear();
	namespace fs = std::filesystem;
	static const std::vector<std::string> kImageExt = { ".png", ".jpg", ".jpeg", ".bmp", ".dds", ".tga" };
	static const std::vector<std::string> kAudioExt  = { ".wav", ".mp3" };
	// Model::Initializeが対応する拡張子（.objは自前パーサー、それ以外はAssimp経由）に合わせる
	static const std::vector<std::string> kModelExt  = { ".obj", ".fbx", ".gltf", ".glb", ".dae" };

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
			// TextureSelector/ModelRenderComponentのInspectorコンボ・保存名の解決に使う
			// 共有一覧（textures_）にも登録しておく。EnsureTextureRegisteredは名前の
			// 重複を見て二重登録しないため、毎回のRescanProjectAssets呼び出しでも安全
			EnsureTextureRegistered(path);
		} else if (std::find(kAudioExt.begin(), kAudioExt.end(), ext) != kAudioExt.end()) {
			projectAudioClips_.push_back({ path, name });
		} else if (std::find(kModelExt.begin(), kModelExt.end(), ext) != kModelExt.end()) {
			projectModels_.push_back({ path, name });
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

ComponentLoadContext SceneBase::MakeComponentLoadContext() {
	ComponentLoadContext ctx;
	ctx.renderer = renderer_;
	ctx.textures = &textures_;
	ctx.ensureTextureRegistered = [this](const std::string& path) { EnsureTextureRegistered(path); };
	ctx.audioClips = &projectAudioClips_;
	return ctx;
}

namespace {
// pathを最後の'/'で「ディレクトリ部（無ければ既定値defaultDirectory）」と「ファイル名部」に分割する。
// AttachAudioAsset（ファイル名部からさらに拡張子を除いたstemが欲しい）とAttachModelAsset
// （ディレクトリ部・ファイル名部の両方が欲しい）で同じ分割ロジックが重複していたため共通化した
struct SplitPathResult {
	std::string directory;
	std::string filename;
};
SplitPathResult SplitPath(const std::string& path, const std::string& defaultDirectory) {
	size_t slashPos = path.find_last_of('/');
	if (slashPos == std::string::npos) return { defaultDirectory, path };
	return { path.substr(0, slashPos), path.substr(slashPos + 1) };
}

// filenameから拡張子（最後の'.'以降）を取り除いた部分を返す（拡張子が無ければfilenameのまま）
std::string StripExtension(const std::string& filename) {
	size_t dot = filename.find_last_of('.');
	return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}
} // namespace

void SceneBase::AttachTextureAsset(GameObject& obj, const std::string& path) {
	// Modelはサブメッシュごとのテクスチャ選択をModelRenderComponent自身のInspectorに内蔵した
	// ため、ドラッグ&ドロップでのTextureSelectorComponent付与は対象外にする
	// （付けても何にも使われず紛らわしいだけのため。Add Componentメニューと同じ理由）
	if (obj.GetComponent<ModelRenderComponent>()) {
		Logger::Log("モデルへの画像ドロップは未対応です。Inspectorのサブメッシュ別テクスチャで選択してください\n");
		return;
	}
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
	ComponentLoadContext ctx = MakeComponentLoadContext();
	nlohmann::json data;
	data["textureName"] = texName;
	ComponentRegistry::Create("TextureSelector", obj, ctx, data);
}

void SceneBase::AttachAudioAsset(GameObject& obj, const std::string& path) {
	std::string filename = SplitPath(path, "").filename;
	std::string stem = StripExtension(filename);

	ComponentLoadContext ctx = MakeComponentLoadContext();
	nlohmann::json data;
	data["filePath"] = path;
	data["registeredName"] = stem;
	data["soundType"] = static_cast<int>(SoundType::SE);
	data["loop"] = false;
	ComponentRegistry::Create("AudioSource", obj, ctx, data);
}

void SceneBase::AttachModelAsset(GameObject& obj, const std::string& path) {
	// 画像/音声と違い、モデルはそれ自体がRenderComponentBase（ModelRenderComponent）を新規に
	// 追加する操作のため、既に何か描画コンポーネントが付いている相手には付与しない
	// （2個目のRenderComponentBaseが付くと、描画ループ・TextureSelectorのどちらからも
	// 「最初の1個」しか見てもらえず無視される壊れた状態になるため。Add Componentメニューの
	// 重複防止ガードと同じ理由）
	if (obj.GetComponent<RenderComponentBase>()) {
		Logger::Log("モデルのドロップは、まだ描画コンポーネントが付いていないGameObjectにのみ行えます\n");
		return;
	}

	// path例: "Resources/Model/teapot/teapot.obj" → directoryPath="Resources/Model/teapot"、
	// filename="teapot.obj"（Model::Initializeの引数形式に合わせる。ModelRenderComponentの
	// Add Componentメニューと同じ組み立て方）
	SplitPathResult split = SplitPath(path, "Resources");
	std::string directoryPath = split.directory;
	std::string filename      = split.filename;

	ComponentLoadContext ctx = MakeComponentLoadContext();
	nlohmann::json data;
	data["directoryPath"] = directoryPath;
	data["filename"] = filename;
	data["hasAnimation"] = false;
	ComponentRegistry::Create("ModelRender", obj, ctx, data);
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

// className用のひな形ヘッダ(.h)の中身を組み立てる。IComponent派生の骨組み
// （Update/DrawImGui/ToJson/FromJson宣言）のみで、実装はソース側に生成する
std::string BuildScriptHeaderContent(const std::string& className) {
	return
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
}

// className用のひな形ソース(.cpp)の中身を組み立てる。各メソッドの空実装＋
// REGISTER_SIMPLE_COMPONENTマクロによるComponentRegistryへの自動登録を含む
std::string BuildScriptSourceContent(const std::string& className, const std::string& typeName,
	const std::string& displayName, const std::string& category) {
	return
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

	std::string headerContent = BuildScriptHeaderContent(className);
	std::string sourceContent = BuildScriptSourceContent(className, typeName, displayName, category);

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

void SceneBase::Render(float deltaTime) {
	lastDeltaTime_ = deltaTime; // HandleSceneTransitionInput（引数なし）側から参照できるよう保持する

	// ImGui::NewFrame()の後、ImGui::Render()の前に呼ぶ必要がある
	ImGuizmo::BeginFrame();

	// エディタUIが非表示の間（＝Releaseビルドの既定状態、またはDebugでF11を押した後）は
	// 常にPlay状態にする。UIが無いと"再生"ボタンを押す手段が無いままisPlaying_==falseに
	// 固定され、Gravity/AutoRun/PlayerController等のコンポーネントが一切更新されなくなるため
	// （viewingGameCamera_をUI非表示時に強制trueにする下のロジックと同じ考え方）
	if (!EditorState::GetInstance().IsUiVisible()) {
		isPlaying_ = true;
	}

	if (isPlaying_) {
		UpdateAutoRunCameraFollowTarget();
	}

	Matrix4x4 view = camera_->GetViewMatrix();
	Matrix4x4 proj = camera_->GetProjectionMatrix(
		camera_->GetAspectRatio(renderer_->GetSceneViewportWidth(), renderer_->GetSceneViewportHeight()));

	// AlphabetTextComponentの子GameObject組み立ては、DeleteObjects/CreateObjectでobjects_自体を
	// 書き換えるため、他のobjects_走査ループの後のこのタイミングで行う
	UpdateAlphabetTextComponents();

	// TextGroupComponentの子GameObject組み立ても同じ理由でこのタイミングで行う
	UpdateTextGroupComponents();

	// DashedLineComponentの子GameObject組み立ても同じ理由でこのタイミングで行う
	UpdateDashedLineComponents();

	GameCameraResolution gameCam = ResolveGameCamera();
	// カメラが無くなったら（削除された等）強制的にSceneへ戻す
	if (!gameCam.gameCamera) viewingGameCamera_ = false;
	// エディタUIを隠している間は常にGameカメラ視点を使う（Sceneの自由カメラ編集は無効化する）
	if (!EditorState::GetInstance().IsUiVisible() && gameCam.gameCamera) viewingGameCamera_ = true;

	// ---- メインパス：Scene（エディタカメラ+Gizmo）とGame（配置カメラ、Gizmoなし）は
	// 画面全体を共有し、ボタンでの選択に応じてどちらか一方だけを描画する ----
	ActiveCameraState activeCam = ResolveActiveCamera(view, proj, gameCam, deltaTime);
	renderer_->SetCamera(activeCam.view, activeCam.proj, activeCam.camPos);
	lastActiveCameraState_ = activeCam; // HandleSceneTransitionInput向けに最新値を控えておく

	float gameplayDeltaTime = ComputeGameplayDeltaTime(deltaTime);

	if (isPlaying_) {
		UpdateGizmoTargets(gameplayDeltaTime, activeCam);
	}

	// ComboPopupComponentの生成・演出更新・破棄はUpdateGizmoTargets（各GameObjectのUpdate、
	// ReflexPlayerComponent等によるtransform.translationの変更を含む）の直後に呼ぶ必要がある。
	// これより前に呼ぶと、頭上オフセット位置の計算が常に1フレーム古いプレイヤー位置を使うことになり、
	// プレイヤーが高速で移動する際にコンボポップアップの位置が追従1フレーム分だけ遅れて見える
	UpdateComboPopupComponents(deltaTime);

	// Gizmoのピッキング/操作はScene表示中のみ（Game表示中は選択・編集させない）
	if (!activeCam.useGameCamera) {
		UpdateGizmoPicking(activeCam);
	}

	// Mirrorを探す（削除されていれば存在しない＝反射パス自体を丸ごとスキップする）。
	// コンポーネント更新（Update）後のTransformで反射させるため、このタイミングで探して描画する
	MirrorResolution mirror = FindMirror();

	if (mirror.mirror) {
		RenderMirrorPass(mirror, view, proj, activeCam);
	}

	RenderMainPass(deltaTime);

	if (mirror.mirror) {
		DrawMirrorObject(mirror);
	}

	// 当たり判定の解決（押し戻し）自体はScene/Gameどちらの表示中でも行うが、
	// ワイヤーフレームのデバッグ描画はScene表示中のみ（drawDebug引数）
	colliderSystem_.ResolveAndDraw(gizmoTargets_, isPlaying_, renderer_, activeCam.view, activeCam.proj, !activeCam.useGameCamera);

	SyncLighting(activeCam);

	// CameraComponentにはメッシュが無く、選択しても向き・画角・映る範囲が分からないため、
	// Blenderのカメラを模したワイヤーフレームで可視化する（Scene表示中のみ。Game表示中は
	// その視点自体を描いていることになるので不要）
	if (!activeCam.useGameCamera) {
		DrawCameraGizmoVisualizations(activeCam);
	}

	// 画面フェード（モザイクセル）はGameObjectを介さない最前面オーバーレイのため、3D/Mirror/Gizmo線の
	// 描画がすべて終わった後、ImGui/シーン遷移判定より前のこのタイミングで直接描く（DrawGrid等の
	// 「シーンが直接Rendererを叩く」既存パターンに倣う）。Updateは毎フレーム進行度を進めるだけの
	// 軽い処理のため、isPlaying_やビュー種別を問わず常に呼んでよい
	FadeManager::GetInstance().Update(deltaTime);
	FadeManager::GetInstance().Draw(renderer_);

	DrawEditorUiIfVisible();

	// クリックによる選択変更はInspector等のImGuiウィジェットが発行された後に判定する
	// （UpdateGizmoPickingLateClickのコメント参照）。Game表示中は従来通り選択・編集させない
	if (!activeCam.useGameCamera) {
		UpdateGizmoPickingLateClick(activeCam);
	}

	ProcessSceneTransitionRequest();
}

void SceneBase::UpdateAutoRunCameraFollowTarget() {
	// CameraFollowComponentのtarget解決：まずタグ"Player"が付いたオブジェクトを優先し、
	// 無ければ（今までどおり）シーン内で最初に見つかったAutoRunComponent持ちオブジェクトに
	// フォールバックする。各オブジェクトのUpdate()より前に解決しておくことで、このフレームの
	// CameraFollowComponent::Updateが古い/nullのtargetを見ないようにする
	GameObject* autoRunTarget = FindObjectByTag(GameTags::kPlayer);
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
}

Renderer::ModelHandle SceneBase::GetOrLoadAlphabetModel(char upperLetter) {
	auto it = alphabetModelCache_.find(upperLetter);
	if (it != alphabetModelCache_.end()) return it->second;

	std::string filename(1, upperLetter);
	filename += ".obj";
	Renderer::ModelHandle handle = renderer_->LoadModel("Resources/Alphabet", filename);
	alphabetModelCache_[upperLetter] = handle;
	return handle;
}

void SceneBase::ClearAlphabetTextChildren(GameObject& owner) {
	// GetChildren()はowner.children_への参照のため、DeleteObjects内でのSetParent(nullptr)により
	// イテレート中に書き換わる。先にコピーを取ってから対象を集める
	std::vector<GameObject*> children = owner.GetChildren();
	std::vector<GameObject*> toDelete;
	for (GameObject* child : children) {
		if (child->tag == GameTags::kAlphabetChar) toDelete.push_back(child);
	}
	if (!toDelete.empty()) DeleteObjects(toDelete);
}

void SceneBase::RebuildAlphabetTextChildren(GameObject& owner, AlphabetTextComponent& comp) {
	// ClearAlphabetTextChildren→DeleteObjectsが末尾でgizmoController_.ResetSelection()を
	// 無条件に呼ぶため、何もせず放置するとInspectorで選択中だったGameObjectが選択解除されて
	// しまう。文字列を1文字打つたび、あるいはtextProviderが毎フレーム値を書き換える動的な
	// AlphabetText（例：残りラウンド数表示）をDragInt等でドラッグ中は毎フレームここを通るため、
	// owner自身が選択中の場合はもちろん、owner以外の全く無関係なGameObject（例：EnemySpawner）を
	// 選択してInspectorを操作している最中でもそのたびに選択が外れてしまっていた。
	// 削除前に選択中オブジェクトを控えておき、再構築後に選択し直す
	GameObject* previouslySelected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	bool previouslySelectedIsRebuiltChild = previouslySelected
		&& previouslySelected->GetParent() == &owner
		&& previouslySelected->tag == GameTags::kAlphabetChar;

	ClearAlphabetTextChildren(owner);

	// 差分検出：今回のtextと前回のlastBuiltTextで共通する先頭部分（common prefix）までは
	// 既に一度演出済みとみなし、演出をやり直さない（毎フレーム/1文字入力するたびに既存の文字まで
	// また奥から出てくるように見える不具合を防ぐ）。単純な前方一致（text全体がlastBuiltTextの
	// 続きになっているか）ではなく共通接頭辞を使うのは、ClearScene::HandleSceneTransitionInputの
	// 名前入力欄のように末尾にカーソル記号"_"を付けて表示する場合、"A_"→"AB_"のような変化が
	// 単純な前方一致（"AB_"が"A_"で始まるか）では成立しないため。共通接頭辞なら"A"の1文字ぶんが
	// 一致していると正しく判定でき、新しく増えた"B"（と入れ替わったカーソル"_"）だけが演出対象になる
	size_t commonPrefixLen = 0;
	size_t maxCompareLen = (std::min)(comp.text.size(), comp.lastBuiltText.size());
	while (commonPrefixLen < maxCompareLen && comp.text[commonPrefixLen] == comp.lastBuiltText[commonPrefixLen]) {
		++commonPrefixLen;
	}
	size_t skipEntranceIndex = commonPrefixLen;

	// 各文字（スペース含む）が占める幅を先に配列化する。スペースだけcomp.spaceWidth、
	// それ以外はcomp.charSpacingを使う（spaceWidthを独立させることで、通常文字の間隔は
	// そのままにスペースだけ広く/狭くできる）。全体の横幅はこの配列の合計になる
	std::vector<float> charWidths(comp.text.size());
	float totalWidth = 0.0f;
	for (size_t i = 0; i < comp.text.size(); ++i) {
		charWidths[i] = (comp.text[i] == ' ') ? comp.spaceWidth : comp.charSpacing;
		totalWidth += charWidths[i];
	}

	// horizontalAlignに応じて左端の開始オフセットを求める。以降は各文字の幅を順に足し込みながら、
	// その文字の中心位置（自分の幅の半分だけ右にずらした位置）を求めていく
	// （等間隔だった旧実装のstartX + spacing*iに相当する累積計算版）。
	// kCenter: 文字列全体の中心がowner（親）のtranslationに来る（従来通り）。
	// kLeft: 1文字目の左端がtranslationに来る（文字数が増減してもtranslationを起点に右へ
	// 伸びるだけになり、既存の文字の位置がずれない。名前入力欄向け）。
	// kRight: 最後の文字の右端がtranslationに来る
	float startX;
	switch (comp.horizontalAlign) {
		case AlphabetTextComponent::HorizontalAlign::kLeft:  startX = 0.0f; break;
		case AlphabetTextComponent::HorizontalAlign::kRight: startX = -totalWidth; break;
		default:                                             startX = -totalWidth * 0.5f; break;
	}
	float cursorX = startX;

	for (size_t i = 0; i < comp.text.size(); ++i) {
		char c = comp.text[i];
		float x = cursorX + charWidths[i] * 0.5f;
		cursorX += charWidths[i];

		if (c == ' ') continue; // スペースは幅分だけ空けて何も生成しない

		char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		bool isLetter = upper >= 'A' && upper <= 'Z';
		bool isDigit = upper >= '0' && upper <= '9'; // 数字はtoupperで変化しないのでcとupperで同じ判定になる
		if (!isLetter && !isDigit) continue; // 対応する.objが無い文字は無視する

		GameObject& charObj = CreateObject(std::string(1, upper));
		charObj.tag = GameTags::kAlphabetChar;
		charObj.excludeFromPicking = true; // 3Dクリックでの誤選択を防ぐ（Hierarchy上では選択・削除可能）
		// text（表示文字列）が変わるたびに作り直される一時的な子GameObjectのため保存対象外にする
		// （保存されてしまうと、次回ロード時にAlphabetTextComponent::lastBuiltTextの初期化と
		// 二重に存在してしまう問題があった。以前はLoadScene直後の一括削除で後始末していたが、
		// そもそも保存しない方が根本的で確実）
		charObj.excludeFromSave = true;
		charObj.SetParent(&owner);
		// SetParent後のtranslationは親からの相対座標として解釈される（GameObject::GetWorldTransform参照）
		charObj.GetTransform().translation = { x, 0.0f, 0.0f };
		charObj.GetTransform().scale = { comp.charScale, comp.charScale, comp.charScale };

		auto* render = charObj.AddComponent<ModelRenderComponent>(GetOrLoadAlphabetModel(upper), false);
		render->directoryPath = "Resources/Alphabet";
		render->filename = std::string(1, upper) + ".obj";

		// 1文字ずつ登場演出：PlayScene::BuildEnemyFromTemplateDataのhasSpawnMove分岐と同じ
		// SpawnMoveComponentを各文字（子GameObject）に個別付与する。startDelayに
		// 「新規に追加された文字の中での順番 × entranceCharDelay」を入れることで、左から右へ
		// 順番に（波及び順で）現れるようにする。translationは親からの相対座標のため、
		// targetPos/startPosもローカル座標（親のtranslationは足さない）のまま扱う。
		// i < skipEntranceIndexの文字（差分検出で「前回までに既に演出済み」と判定された先頭部分）は
		// 演出を適用せず、SpawnMoveComponentを付けずに最初から定位置へ直接配置する
		if (comp.useCharEntranceAnimation && i >= skipEntranceIndex) {
			Vector3 targetPos = charObj.GetTransform().translation;
			auto* spawnMove = charObj.AddComponent<SpawnMoveComponent>();
			spawnMove->targetPos = targetPos;
			spawnMove->startPos = targetPos + Vector3{ 0.0f, 0.0f, comp.entranceZOffset };
			spawnMove->duration = comp.entranceDuration;
			spawnMove->easing = comp.entranceEasing;
			spawnMove->startDelay = comp.entranceCharDelay * static_cast<float>(i - skipEntranceIndex);
			spawnMove->elapsed = 0.0f;
			spawnMove->finished = false; // 既定値trueのため明示的にfalseへ戻して演出を開始する
			charObj.GetTransform().translation = spawnMove->startPos;
		}
	}

	comp.lastBuiltText = comp.text;
	comp.lastBuiltCharScale = comp.charScale;
	comp.lastBuiltCharSpacing = comp.charSpacing;
	comp.lastBuiltSpaceWidth = comp.spaceWidth;
	comp.lastBuiltHorizontalAlign = comp.horizontalAlign;
	RebuildDerivedLists(); // 新規生成した子をgizmoTargets_に反映する

	// 選択復元：previouslySelectedが今回の削除対象（文字の子）自身だった場合はもう存在しないため
	// 復元しようがない（その場合は選択解除のままでよい）。それ以外（owner自身、または
	// このAlphabetTextとは無関係な別のGameObjectを選択していた場合）はClearAlphabetTextChildren/
	// CreateObjectの対象外なので生存しており、正しく復元できる
	if (previouslySelected && !previouslySelectedIsRebuiltChild) {
		bool is2D = previouslySelected->GetComponent<TransformComponent>()->is2D;
		if (is2D) {
			gizmoController_.SetSelected2D(previouslySelected, screenTargets_);
		} else {
			gizmoController_.SetSelected(previouslySelected, gizmoTargets_);
		}
	}
}

void SceneBase::UpdateAlphabetTextComponents() {
	// AlphabetTextComponent::text/charScale/charSpacing/spaceWidthのいずれかが前回組み立てた時点の値
	// （lastBuiltText/lastBuiltCharScale/lastBuiltCharSpacing/lastBuiltSpaceWidth）と変わっていたら
	// 子GameObjectを作り直す。変わっていなければ何もしない（毎フレームDeleteObjects/CreateObjectを
	// 繰り返さないようにするため）。textだけを見ていると、Inspectorで文字間隔・文字サイズだけを
	// 変更しても反映されず「次に文字を1つ追加/削除した瞬間にまとめて反映される」ように見えて
	// しまっていたため、4つとも比較対象にする
	// RebuildAlphabetTextChildrenはDeleteObjects/CreateObject経由でobjects_自体（vector）を
	// 書き換えるため、objects_をイテレート中に直接呼ぶとイテレータが無効化される
	// （PlayScene::ProcessPendingDestroysと同じ理由）。先に対象だけ集め、ループを抜けてから処理する
	std::vector<std::pair<GameObject*, AlphabetTextComponent*>> toRebuild;
	for (auto& obj : objects_) {
		if (auto* comp = obj->GetComponent<AlphabetTextComponent>()) {
			// textProviderが設定されていれば、比較の前にtextへ反映しておく（TextRenderComponent::
			// UpdateDynamicTextと同じ「毎フレーム呼んで中身を最新化してから使う」パターン）
			comp->UpdateTextFromProvider();

			bool changed = comp->text != comp->lastBuiltText
				|| comp->charScale != comp->lastBuiltCharScale
				|| comp->charSpacing != comp->lastBuiltCharSpacing
				|| comp->spaceWidth != comp->lastBuiltSpaceWidth
				|| comp->horizontalAlign != comp->lastBuiltHorizontalAlign;
			if (changed) {
				toRebuild.push_back({ obj.get(), comp });
			}

			// displayScaleMultiplier/displayColorは子GameObjectの再構築を伴わない軽量な演出反映
			// （PlayButtonComponentのホバー演出等が毎フレーム書き換える想定）。
			// ownerのTransform.scaleに倍率をかけると全文字がまとめて拡縮され、各文字の子GameObjectが
			// 持つModelRenderComponent::colorに色を反映する。
			// ただし、ownerにSpawnMoveComponent::animateScale==trueが付いている間は、この上書きを
			// スキップする。UpdateAlphabetTextComponentsはUpdateGizmoTargets（SpawnMoveComponent::
			// Updateを含む）より先に実行されるため、ここで無条件にscaleを書き込んでしまうと、
			// 「前フレームでSpawnMoveComponentが計算した正しいscale」を次のフレームの冒頭で
			// 毎回displayScaleMultiplier（等倍固定）へ巻き戻してしまい、SpawnMoveComponent側の
			// scaleアニメーション（0→targetScaleへの拡大演出）が実質何も反映されなくなっていた
			// （translationはこのブロックで一切触らないため巻き戻らず、scaleだけが効かない
			// 非対称な症状になっていた）
			auto* spawnMoveForScale = obj->GetComponent<SpawnMoveComponent>();
			bool skipScaleOverride = spawnMoveForScale && spawnMoveForScale->animateScale;
			if (!skipScaleOverride) {
				obj->GetTransform().scale = { comp->displayScaleMultiplier, comp->displayScaleMultiplier, comp->displayScaleMultiplier };
			}
			for (GameObject* child : obj->GetChildren()) {
				if (child->tag != GameTags::kAlphabetChar) continue;
				if (auto* render = child->GetComponent<ModelRenderComponent>()) {
					render->color = comp->displayColor;
				}
			}
		}
	}
	for (auto& [owner, comp] : toRebuild) {
		RebuildAlphabetTextChildren(*owner, *comp);
	}
}

void SceneBase::ClearTextGroupChildren(GameObject& owner) {
	// ClearAlphabetTextChildrenと全く同じ理由・同じ実装パターン（GetChildren()はDeleteObjects内での
	// SetParent(nullptr)によりイテレート中に書き換わるため、先にコピーを取ってから対象を集める）
	std::vector<GameObject*> children = owner.GetChildren();
	std::vector<GameObject*> toDelete;
	for (GameObject* child : children) {
		if (child->tag == GameTags::kTextGroupEntry) toDelete.push_back(child);
	}
	if (!toDelete.empty()) DeleteObjects(toDelete);
}

void SceneBase::RebuildTextGroupChildren(GameObject& owner, TextGroupComponent& comp) {
	// RebuildAlphabetTextChildrenと同じ理由：ClearTextGroupChildren→DeleteObjectsが末尾で
	// gizmoController_.ResetSelection()を無条件に呼ぶため、削除前に選択中オブジェクトを控えておき、
	// 再構築後に選択し直す
	GameObject* previouslySelected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	bool previouslySelectedIsRebuiltChild = previouslySelected
		&& previouslySelected->GetParent() == &owner
		&& previouslySelected->tag == GameTags::kTextGroupEntry;

	ClearTextGroupChildren(owner);

	ComponentLoadContext ctx = MakeComponentLoadContext();
	for (size_t i = 0; i < comp.entries.size(); ++i) {
		const TextGroupComponent::Entry& entry = comp.entries[i];

		GameObject& entryObj = CreateObject("TextGroupEntry" + std::to_string(i));
		entryObj.tag = GameTags::kTextGroupEntry;
		// entries（表示内容）が変わるたびに作り直される一時的な子GameObjectのため保存対象外にする
		// （AlphabetTextComponentの子と同じ理由）
		entryObj.excludeFromSave = true;
		entryObj.SetParent(&owner);

		// stackDirectionの方向へi個ぶんspacing間隔だけずらした位置に並べる。SetParent後の
		// translationは親からの相対座標として解釈される（GameObject::GetWorldTransform参照）
		Vector3 offset = comp.anchorOffset;
		float advance = comp.spacing * static_cast<float>(i);
		if (comp.stackDirection == TextGroupComponent::StackDirection::kVertical) {
			offset.y += advance;
		} else {
			offset.x += advance;
		}
		entryObj.GetTransform().translation = offset;

		// TextSpriteComponentはRebuild(renderer)をAddComponent直後に呼ぶ必要があるため
		// （テクスチャのラスタライズ）、REGISTER_SIMPLE_COMPONENTではなくComponentRegistry::Create
		// 経由で生成する（ComponentRegistration.cppのcreator参照）。data経由でtext/fontSize/
		// horizontalAlign/colorを渡せば、Inspectorの「保存」ボタンを介さず直接確定させられる
		nlohmann::json data;
		data["text"] = entry.text;
		data["fontSize"] = entry.fontSize;
		data["horizontalAlign"] = static_cast<int>(entry.horizontalAlign);
		data["color"] = Vector4ToJson(entry.color);
		ComponentRegistry::Create("TextSprite", entryObj, ctx, data);

		// TextSpriteComponentは常にTransformComponent::is2D==trueのスクリーン空間で使う想定
		// （DrawAddTextSpriteNode参照）。ownerと同じ2D空間に子を配置する
		entryObj.GetComponent<TransformComponent>()->is2D = true;
	}

	comp.lastBuiltEntries = comp.entries;
	comp.lastBuiltAnchorOffset = comp.anchorOffset;
	comp.lastBuiltSpacing = comp.spacing;
	comp.lastBuiltStackDirection = comp.stackDirection;
	comp.builtOnce = true;
	RebuildDerivedLists(); // 新規生成した子をscreenTargets_等に反映する

	// 選択復元：RebuildAlphabetTextChildrenと同じロジック
	if (previouslySelected && !previouslySelectedIsRebuiltChild) {
		bool is2D = previouslySelected->GetComponent<TransformComponent>()->is2D;
		if (is2D) {
			gizmoController_.SetSelected2D(previouslySelected, screenTargets_);
		} else {
			gizmoController_.SetSelected(previouslySelected, gizmoTargets_);
		}
	}
}

void SceneBase::UpdateTextGroupComponents() {
	// entries/anchorOffset/spacing/stackDirectionのいずれかが前回組み立てた時点の値と変わっていたら
	// 子GameObjectを作り直す（UpdateAlphabetTextComponentsと同じ変更検知パターン）。
	// RebuildTextGroupChildrenはDeleteObjects/CreateObject経由でobjects_自体（vector）を書き換える
	// ため、objects_をイテレート中に直接呼ぶとイテレータが無効化される。先に対象だけ集め、
	// ループを抜けてから処理する
	std::vector<std::pair<GameObject*, TextGroupComponent*>> toRebuild;
	for (auto& obj : objects_) {
		if (auto* comp = obj->GetComponent<TextGroupComponent>()) {
			bool changed = !comp->builtOnce
				|| comp->entries != comp->lastBuiltEntries
				|| comp->anchorOffset.x != comp->lastBuiltAnchorOffset.x
				|| comp->anchorOffset.y != comp->lastBuiltAnchorOffset.y
				|| comp->anchorOffset.z != comp->lastBuiltAnchorOffset.z
				|| comp->spacing != comp->lastBuiltSpacing
				|| comp->stackDirection != comp->lastBuiltStackDirection;
			if (changed) {
				toRebuild.push_back({ obj.get(), comp });
			}
		}
	}
	for (auto& [owner, comp] : toRebuild) {
		RebuildTextGroupChildren(*owner, *comp);
	}
}

void SceneBase::ClearDashedLineSegments(GameObject& owner) {
	// AlphabetTextComponentのClearAlphabetTextChildrenと全く同じ理由・同じ実装パターン
	// （GetChildren()はDeleteObjects内でのSetParent(nullptr)によりイテレート中に書き換わるため、
	// 先にコピーを取ってから対象を集める）
	std::vector<GameObject*> children = owner.GetChildren();
	std::vector<GameObject*> toDelete;
	for (GameObject* child : children) {
		if (child->tag == GameTags::kDashedLineSegment) toDelete.push_back(child);
	}
	if (!toDelete.empty()) DeleteObjects(toDelete);
}

void SceneBase::RebuildDashedLineSegments(GameObject& owner, DashedLineComponent& comp) {
	// AlphabetTextComponentのRebuildAlphabetTextChildrenと同じ「削除前に選択中オブジェクトを
	// 控えておき、再構築後に選択し直す」対策（ClearDashedLineSegments→DeleteObjectsが末尾で
	// gizmoController_.ResetSelection()を無条件に呼ぶため）
	GameObject* previouslySelected = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	bool previouslySelectedIsRebuiltChild = previouslySelected
		&& previouslySelected->GetParent() == &owner
		&& previouslySelected->tag == GameTags::kDashedLineSegment;

	ClearDashedLineSegments(owner);

	int count = (std::max)(comp.dashCount, 0);
	if (count > 0) {
		// 中心がowner（親）のtranslationに来るよう左端の開始位置を求める。N本のダッシュ中心は
		// -halfSpan+spacing/2 から spacing間隔で +halfSpan-spacing/2 まで並ぶ
		// （AlphabetTextComponentの旧・等間隔実装と同じ計算式）
		float totalSpan = static_cast<float>(count) * comp.dashSpacing;
		float startX = -totalSpan * 0.5f + comp.dashSpacing * 0.5f;

		for (int i = 0; i < count; i++) {
			float x = startX + comp.dashSpacing * static_cast<float>(i);

			GameObject& dashObj = CreateObject("下線ダッシュ" + std::to_string(i));
			dashObj.tag = GameTags::kDashedLineSegment;
			dashObj.excludeFromPicking = true; // 3Dクリックでの誤選択を防ぐ（Hierarchy上では選択・削除可能）
			// dashCount/dashSpacingが変わるたびに作り直される一時的な子GameObjectのため
			// 保存対象外にする（AlphabetTextComponentの子と同じ理由）
			dashObj.excludeFromSave = true;
			dashObj.SetParent(&owner);
			dashObj.GetTransform().translation = { x, 0.0f, 0.0f };
			dashObj.GetTransform().scale = { comp.dashWidth, comp.dashThickness, comp.dashThickness };

			auto* render = dashObj.AddComponent<CubeRenderComponent>();
			render->color = comp.color;
			render->lighting = false;
		}
	}

	comp.lastBuiltDashCount = comp.dashCount;
	comp.lastBuiltDashWidth = comp.dashWidth;
	comp.lastBuiltDashThickness = comp.dashThickness;
	comp.lastBuiltDashSpacing = comp.dashSpacing;
	RebuildDerivedLists(); // 新規生成した子をgizmoTargets_に反映する

	// 選択復元：RebuildAlphabetTextChildrenと同じロジック
	if (previouslySelected && !previouslySelectedIsRebuiltChild) {
		bool is2D = previouslySelected->GetComponent<TransformComponent>()->is2D;
		if (is2D) {
			gizmoController_.SetSelected2D(previouslySelected, screenTargets_);
		} else {
			gizmoController_.SetSelected(previouslySelected, gizmoTargets_);
		}
	}
}

void SceneBase::UpdateDashedLineComponents() {
	// AlphabetTextComponentのUpdateAlphabetTextComponentsと同じ「変更検知→まとめて再構築」パターン。
	// RebuildDashedLineSegmentsはDeleteObjects/CreateObject経由でobjects_自体（vector）を書き換える
	// ため、objects_をイテレート中に直接呼ぶとイテレータが無効化される。先に対象だけ集め、
	// ループを抜けてから処理する
	std::vector<std::pair<GameObject*, DashedLineComponent*>> toRebuild;
	for (auto& obj : objects_) {
		if (auto* comp = obj->GetComponent<DashedLineComponent>()) {
			bool changed = comp->dashCount != comp->lastBuiltDashCount
				|| comp->dashWidth != comp->lastBuiltDashWidth
				|| comp->dashThickness != comp->lastBuiltDashThickness
				|| comp->dashSpacing != comp->lastBuiltDashSpacing;
			if (changed) {
				toRebuild.push_back({ obj.get(), comp });
			}
		}
	}
	for (auto& [owner, comp] : toRebuild) {
		RebuildDashedLineSegments(*owner, *comp);
	}
}

void SceneBase::SpawnComboPopup(GameObject& owner, ComboPopupComponent& comp, int comboValue) {
	// 数字文字列（負数は本来使わない想定だが、万一マイナスが来ても不正な.obj参照にならないよう
	// '-'は他の非対応文字と同じく無視する扱いにする＝absを取って処理する）
	std::string digits = std::to_string(comboValue < 0 ? -comboValue : comboValue);

	// ポップアップ全体を表す親GameObject。プレイヤー(owner)の子にはせず、ルートに独立して置く
	// （子にすると、プレイヤーの回転がそのままオフセット位置・見た目の向きに伝播してしまい、
	// それを打ち消す行列計算が必要になって複雑・不安定だった。UpdateComboPopupComponentsが
	// 毎フレーム「プレイヤーのワールド位置 + baseYOffset」を直接translationに代入するだけで
	// 済むようにするため、最初から親子関係を持たせない）。
	// scale/alphaはUpdateComboPopupComponentsがelapsed経過に応じて毎フレーム書き換える
	GameObject& group = CreateObject("ComboPopup " + digits);
	group.tag = GameTags::kComboPopup;
	group.excludeFromPicking = true; // 演出用オブジェクトなので3Dクリック選択の対象外にする
	// 短い寿命で自動的に消える一時的な演出オブジェクトのため保存対象外にする（詳しくは
	// RebuildAlphabetTextChildrenの同様のコメント参照）
	group.excludeFromSave = true;
	group.GetTransform().translation = owner.GetWorldTransform().translation + Vector3{ 0.0f, comp.baseYOffset, 0.0f };
	group.GetTransform().rotation = { 0.0f, 0.0f, 0.0f }; // 常にカメラに対して同じ向き（回転しない）
	group.GetTransform().scale = { 0.0f, 0.0f, 0.0f }; // ポップイン演出の初期値（0から拡大する）

	// 桁を横一列に並べる（AlphabetTextComponent/RebuildAlphabetTextChildrenと同じ「中心揃え」ロジック）。
	// 間隔はcomp.digitSpacing（Inspectorで調整可能）を使う
	float totalWidth = static_cast<float>(digits.size()) * comp.digitSpacing;
	float startX = -totalWidth * 0.5f + comp.digitSpacing * 0.5f;

	for (size_t i = 0; i < digits.size(); ++i) {
		char digitChar = digits[i];
		float x = startX + comp.digitSpacing * static_cast<float>(i);

		GameObject& digitObj = CreateObject(std::string(1, digitChar));
		digitObj.excludeFromPicking = true;
		// 親のgroupがexcludeFromSaveでも、自分（子）は独立してフィルタ判定されるため
		// 明示的に指定する必要がある（指定し忘れると、親を失った孤立オブジェクトとして
		// parentIndex=-1で保存されてしまう）
		digitObj.excludeFromSave = true;
		digitObj.SetParent(&group);
		digitObj.GetTransform().translation = { x, 0.0f, 0.0f };
		digitObj.GetTransform().scale = { 1.0f, 1.0f, 1.0f }; // 拡縮はgroup側のscaleだけで行う（子は等倍のまま）

		auto* render = digitObj.AddComponent<ModelRenderComponent>(GetOrLoadAlphabetModel(digitChar), false);
		render->directoryPath = "Resources/Alphabet";
		render->filename = std::string(1, digitChar) + ".obj";
	}

	comp.activePopup_ = { comboValue, &group, 0.0f };
}

void SceneBase::UpdateComboPopupComponents(float deltaTime) {
	// 生成・削除の対象（GameObject*）を先に集めてからループの外で処理する。ClearAlphabetTextChildren/
	// RebuildAlphabetTextChildrenと同じ理由（DeleteObjects/CreateObjectがobjects_自体を書き換えるため、
	// objects_をイテレート中に直接呼ぶとイテレータが無効化される）
	struct SpawnRequest { GameObject* owner; ComboPopupComponent* comp; int comboValue; };
	std::vector<SpawnRequest> toSpawn;
	std::vector<GameObject*> toDestroy; // ConsumeClearRequested()、または新しい値への差し替えで消える分

	for (auto& obj : objects_) {
		auto* comp = obj->GetComponent<ComboPopupComponent>();
		if (!comp) continue;

		int requestedValue = 0;
		bool hasRequest = comp->ConsumePendingRequest(requestedValue);
		bool clearRequested = comp->ConsumeClearRequested();

		// 新しい値のリクエスト・明示的なクリアのどちらでも、表示中のポップアップがあれば
		// 一旦破棄する（キルカウントHUDと同じ「1つの表示が値の更新に合わせて差し替わる」方式。
		// 同じ値が連続で来た場合もポップインをやり直したいので、値の比較はせず常に破棄→再生成する）
		if ((hasRequest || clearRequested) && comp->activePopup_.modelObject) {
			toDestroy.push_back(comp->activePopup_.modelObject);
			comp->activePopup_ = ComboPopupComponent::ActivePopup{};
		}

		if (hasRequest) {
			toSpawn.push_back({ obj.get(), comp, requestedValue });
		}
	}

	if (!toDestroy.empty()) DeleteObjects(toDestroy);
	for (auto& req : toSpawn) {
		SpawnComboPopup(*req.owner, *req.comp, req.comboValue);
	}

	// 表示中のポップアップのelapsedを進め、scale/alphaをイージングで更新する。寿命が尽きたものは
	// このループでは消さず、対象だけ集めて後でまとめてDeleteObjectsする（同じイテレータ無効化対策）
	std::vector<GameObject*> toExpire;
	for (auto& obj : objects_) {
		auto* comp = obj->GetComponent<ComboPopupComponent>();
		if (!comp || !comp->activePopup_.modelObject) continue;

		ComboPopupComponent::ActivePopup& popup = comp->activePopup_;
		popup.elapsed += deltaTime;
		float lifetime = comp->popInDuration + comp->holdDuration + comp->fadeOutDuration;

		if (popup.elapsed >= lifetime) {
			toExpire.push_back(popup.modelObject);
			comp->activePopup_ = ComboPopupComponent::ActivePopup{};
			continue;
		}

		float scale;
		float alpha;
		if (popup.elapsed < comp->popInDuration) {
			// ポップイン中：0→charScaleへイージングで拡大。alphaは常に不透明
			float t = comp->popInDuration > 0.0f ? popup.elapsed / comp->popInDuration : 1.0f;
			scale = comp->charScale * Easing::Apply(comp->popInEasing, t);
			alpha = 1.0f;
		} else if (popup.elapsed < comp->popInDuration + comp->holdDuration) {
			// 静止表示中：サイズ固定、不透明のまま。次のコンボがこの時間内に来なければ、
			// このelapsedが伸び続けて次のフェーズ（フェードアウト）へ自然に進む
			scale = comp->charScale;
			alpha = 1.0f;
		} else {
			// フェードアウト中：サイズ固定のまま、alphaだけ1→0へイージングで減少
			float fadeElapsed = popup.elapsed - comp->popInDuration - comp->holdDuration;
			float t = comp->fadeOutDuration > 0.0f ? fadeElapsed / comp->fadeOutDuration : 1.0f;
			scale = comp->charScale;
			alpha = 1.0f - Easing::Apply(comp->fadeOutEasing, t);
		}

		popup.modelObject->GetTransform().scale = { scale, scale, scale };

		// popup.modelObject（group）はプレイヤー（obj）の子GameObjectにしていない（SpawnComboPopup
		// 参照）ため、プレイヤーの回転をそもそも一切継承しない。毎フレーム「プレイヤーの現在の
		// ワールド位置 + baseYOffset」を直接ワールド座標として代入するだけでよく、回転を打ち消す
		// ための行列計算は不要（回転は常に既定値{0,0,0}のまま変更しない）
		popup.modelObject->GetTransform().translation =
			obj->GetWorldTransform().translation + Vector3{ 0.0f, comp->baseYOffset, 0.0f };
		// alphaは桁ごとの子GameObject（ModelRenderComponent::color.a）に反映する。
		// groupObject自身は見た目を持たない空のGameObjectのためcolorを持たない
		for (GameObject* digitObj : popup.modelObject->GetChildren()) {
			if (auto* render = digitObj->GetComponent<ModelRenderComponent>()) {
				render->color.w = alpha;
				render->blendMode = BlendMode::kNormal; // alphaブレンドを有効化しないと透明化が見た目に反映されない
			}
		}
	}
	if (!toExpire.empty()) DeleteObjects(toExpire);

	if (!toDestroy.empty() || !toSpawn.empty() || !toExpire.empty()) {
		RebuildDerivedLists();
	}
}

SceneBase::GameCameraResolution SceneBase::ResolveGameCamera() {
	// Gameモード用カメラの候補を探す：まずタグ"MainCamera"が付いたオブジェクトを優先し、
	// 無ければ（今までどおり）シーン内で最初に見つかったCameraComponentにフォールバックする。
	// 複数カメラがある場合にどれを使うか明示的に選べるように、Unityの「MainCameraタグ」相当を追加した
	GameCameraResolution result;
	if (GameObject* tagged = FindObjectByTag(GameTags::kMainCamera)) {
		if (auto* c = tagged->GetComponent<CameraComponent>()) {
			result.gameCamera = c;
			result.gameCameraObject = tagged;
		}
	}
	if (!result.gameCamera) {
		for (auto& obj : objects_) {
			if (auto* c = obj->GetComponent<CameraComponent>()) {
				result.gameCamera = c;
				result.gameCameraObject = obj.get();
				break;
			}
		}
	}
	return result;
}

SceneBase::ActiveCameraState SceneBase::ResolveActiveCamera(const Matrix4x4& view, const Matrix4x4& proj,
	const GameCameraResolution& gameCam, float deltaTime) {
	bool useGameCamera = viewingGameCamera_ && gameCam.gameCamera != nullptr;

	ActiveCameraState result;
	result.view = view;
	result.proj = proj;
	result.camPos = camera_->GetCameraData().position;
	result.useGameCamera = useGameCamera;
	if (useGameCamera) {
		// GetWorldTransform()（ImGuizmoのatan2ベースEuler分解経由）ではなくGetWorldMatrix()を
		// 直接渡す。Euler往復変換だとyawが±180度付近を通過する瞬間に分解結果が不連続にジャンプし、
		// カメラの向きが突然反転して見える不具合があったため（GamepadCameraLookComponent参照）
		Matrix4x4 camWorldMatrix = gameCam.gameCameraObject->GetWorldMatrix();
		result.view = gameCam.gameCamera->GetViewMatrixFromWorld(camWorldMatrix);
		result.proj = gameCam.gameCamera->GetProjectionMatrix(
			camera_->GetAspectRatio(renderer_->GetSceneViewportWidth(), renderer_->GetSceneViewportHeight()));
		// positionOffset込みの実際の視点位置（ライティングの鏡面反射計算等で使われる）
		result.camPos = gameCam.gameCamera->GetEffectiveWorldPositionFromWorld(camWorldMatrix);
	}
	// 敵ヒット時のカメラシェイク：ReflexEnemyComponent::OnTriggerEnter等がHitEffect::RequestShakeで
	// 積んだ要求を消費し、ランダムなオフセットをカメラ位置に加算する。isPlaying_中のみ（Stop中の
	// エディタ編集画面が揺れると作業しづらいため）。実時間（deltaTime）で減衰させるため、
	// ヒットストップでdeltaTimeを書き換える前のここで呼ぶ。
	// view行列を直接並進させる式を組み立てる代わりに、view行列の逆行列（＝カメラのワールド行列）の
	// 並進成分へオフセットを加算してから再度逆行列を取ることで、Scene/Gameどちらのカメラでも
	// 回転成分を気にせず同じ式で処理できるようにしている
	if (isPlaying_) {
		Vector3 shakeOffset = HitEffect::ConsumeShakeOffset(deltaTime);
		if (shakeOffset.x != 0.0f || shakeOffset.y != 0.0f || shakeOffset.z != 0.0f) {
			Matrix4x4 camWorld = MatrixMath::Inverse(result.view);
			camWorld.m[3][0] += shakeOffset.x;
			camWorld.m[3][1] += shakeOffset.y;
			camWorld.m[3][2] += shakeOffset.z;
			result.view = MatrixMath::Inverse(camWorld);
			result.camPos = result.camPos + shakeOffset;
		}
	}
	return result;
}

float SceneBase::ComputeGameplayDeltaTime(float deltaTime) const {
	// 敵ヒット時のヒットストップ：残り時間中はコンポーネントに渡すdeltaTimeを0にして
	// 見た目上の動きを止める（実時間でのタイマー消化はHitEffect::IsHitStopActive内で行う）
	if (isPlaying_ && HitEffect::IsHitStopActive(deltaTime)) {
		return 0.0f;
	}
	return deltaTime;
}

void SceneBase::UpdateGizmoTargets(float gameplayDeltaTime, const ActiveCameraState& activeCam) {
	// isPlaying_中のみコンポーネント更新（Stop中はGizmoで自由に配置できるようにする）。
	// activeView/activeProj確定後に呼ぶことで、ReflexPlayerComponent等クリック→ワールド座標変換に
	// Renderer/view/projを要するコンポーネントにもUpdateContext経由で渡せるようにしている
	// （以前はカメラ計算前の位置で呼んでいたが、Renderer情報を必要としないコンポーネントの
	// 挙動には影響しない）。isGameViewはGizmoController::UpdatePicking等と同じuseGameCamera値を
	// 渡す。Sceneビュー表示中はGizmoのオブジェクト選択・矩形選択が同じ左クリックを使うため、
	// クリックでゲームロジックを動かすコンポーネント側でも二重にガードできるようにする
	UpdateContext updateCtx{ renderer_, activeCam.view, activeCam.proj, activeCam.useGameCamera, &gizmoTargets_ };
	for (auto* obj : gizmoTargets_) obj->Update(gameplayDeltaTime, updateCtx);
}

void SceneBase::UpdateGizmoPicking(const ActiveCameraState& activeCam) {
	// クリックによる選択変更（UpdatePicking/UpdatePicking2D）は行わず、既存選択のドラッグ編集
	// （UpdateGizmo/UpdateGizmo2D、ImGuizmo::Manipulateの結果をTransformへ即時反映する）だけを行う。
	// ピッキングを分離した理由はUpdateGizmoPickingLateClickのコメントを参照
	gizmoController_.UpdateGizmo(gizmoTargets_, renderer_, activeCam.view, activeCam.proj);
	gizmoController_.UpdateGizmo2D(screenTargets_, renderer_);
	gizmoController_.UpdateContextMenu(renderer_);
}

void SceneBase::UpdateGizmoPickingLateClick(const ActiveCameraState& activeCam) {
	// UpdatePicking/UpdatePicking2D（クリックの立ち上がり検知）は、このフレームでDrawImGui()が
	// Inspector等のウィジェットを実際に発行し終えた後に呼ぶ必要がある。ImGui::GetIO().
	// WantCaptureMouseは「ウィジェットが発行された時点」で確定するため、DrawImGui()より前に
	// ピッキングを行うと常に前フレーム終了時点の（1フレーム遅延した）WantCaptureMouseを見ることになる。
	// これが原因で、InspectorのDragInt等を外側からクリックし始めた最初のフレームで
	// WantCaptureMouseがまだfalseのままとなり、3D/2Dピッキングが誤発火して「値を変更しようとした
	// 瞬間に選択が解除される」不具合になっていた。Gizmoのドラッグ編集自体（UpdateGizmoPicking内の
	// UpdateGizmo/UpdateGizmo2D）はTransform即時反映のため従来通りRenderMainPassより前で行い、
	// 選択を切り替えるクリック判定だけをこちらに分離した
	gizmoController_.UpdatePicking(gizmoTargets_, renderer_, activeCam.view, activeCam.proj);
	gizmoController_.UpdatePicking2D(screenTargets_, renderer_);
}

SceneBase::MirrorResolution SceneBase::FindMirror() {
	MirrorResolution result;
	for (auto& obj : objects_) {
		if (auto* m = obj->GetComponent<MirrorComponent>()) {
			result.mirror = m;
			result.mirrorObject = obj.get();
			break;
		}
	}
	return result;
}

void SceneBase::RenderMirrorPass(const MirrorResolution& mirror, const Matrix4x4& view, const Matrix4x4& proj,
	const ActiveCameraState& activeCam) {
	// 鏡の反射視点で、Mirror自身とSprite2D（スクリーン空間UI）以外を全部オフスクリーンへ描画する
	Collision::Plane mirrorPlane   = mirror.mirror->ComputePlane(mirror.mirrorObject->GetTransform());
	Matrix4x4        reflection    = MatrixMath::MakeReflectionMatrix(mirrorPlane);
	Matrix4x4        reflectedView = reflection * view;
	Vector3          reflectedCamPos = TransformMath::Transform(camera_->GetCameraData().position, reflection);

	renderer_->BeginMirrorPass(reflectedView, proj, reflectedCamPos);
	for (auto& obj : objects_) {
		if (obj->GetComponent<MirrorComponent>()) continue;
		if (auto* sprite = obj->GetComponent<SpriteRenderComponent>()) {
			if (!sprite->is3D) continue; // Sprite2Dは反射に映さない
		}
		// GetComponents（複数形）：TextRenderComponentのように1GameObjectに複数付けられる型を
		// 漏れなく描画するため、「最初の1個」ではなく該当する全RenderComponentBaseを回す
		for (auto* r : obj->GetComponents<RenderComponentBase>()) {
			// deltaTime=0：ModelRenderComponentのアニメーションを通常パスと二重に進めないため
			r->Draw(renderer_, obj->GetWorldTransform(), 0.0f);
		}
	}
	renderer_->EndMirrorPass();
	// オフスクリーンパス中にSetCameraの内容が上書きされているため、メインパス用に戻す
	renderer_->SetCamera(activeCam.view, activeCam.proj, activeCam.camPos);
}

void SceneBase::RenderMainPass(float deltaTime) {
	for (auto& obj : objects_) {
		if (obj->GetComponent<MirrorComponent>()) continue; // Mirrorは反射テクスチャ確定後に描画する
		// GetComponents（複数形）：TextRenderComponentのように1GameObjectに複数付けられる型を
		// 漏れなく描画するため、「最初の1個」ではなく該当する全RenderComponentBaseを回す
		for (auto* r : obj->GetComponents<RenderComponentBase>()) {
			r->Draw(renderer_, obj->GetWorldTransform(), deltaTime);
		}
	}
}

void SceneBase::DrawMirrorObject(const MirrorResolution& mirror) {
	mirror.mirror->Draw(renderer_, mirror.mirrorObject->GetWorldTransform());
}

void SceneBase::SyncLighting(const ActiveCameraState& activeCam) {
	// 各ライトコンポーネントが自分のSetter呼び出しとデバッグ表示を行う（ILightComponent経由で汎用処理）。
	// SyncToRenderer（実際のライティング反映）はどちらの表示中でも必要、デバッグ可視化はScene表示中のみ。
	// ループ前に一旦Directional/Point/Spotの有効フラグを全部リセットしておく（削除されたコンポーネントの
	// 光源が有効なまま残り続けるのを防ぐ。詳細はResetPerFrameEnableFlagsのコメント参照）。
	// Point/SpotLightは複数配置できるため、種類ごとに「今フレーム何番目に見つかったか」を数えて
	// SceneLightの配列インデックスとして渡す（GetLightType()でダウンキャストせずに種類判定する）
	renderer_->GetLight().ResetPerFrameEnableFlags();
	uint32_t lightSlotCounters[3] = { 0, 0, 0 }; // kDirectional/kPoint/kSpotの順（LightType参照）
	for (auto& obj : objects_) {
		if (auto* light = obj->GetComponent<ILightComponent>()) {
			uint32_t& slot = lightSlotCounters[static_cast<int>(light->GetLightType())];
			light->SyncToRenderer(renderer_, obj->GetWorldTransform(), slot);
			slot++;
			if (!activeCam.useGameCamera) {
				light->DrawGizmoVisualization(renderer_, obj->GetWorldTransform(), activeCam.view, activeCam.proj);
			}
		}
	}
}

void SceneBase::DrawCameraGizmoVisualizations(const ActiveCameraState& activeCam) {
	float cameraGizmoAspect = camera_->GetAspectRatio(renderer_->GetSceneViewportWidth(), renderer_->GetSceneViewportHeight());
	for (auto& obj : objects_) {
		if (auto* cam = obj->GetComponent<CameraComponent>()) {
			// positionOffset/rotationOffset込みの「実際に見ている位置」にアイコンを描く
			// （オーナーの生のTransformのまま描くと、オフセットで実際の視点とズレて表示される）
			Transform effective = cam->GetEffectiveWorldTransform(obj->GetWorldTransform());
			cam->DrawGizmoVisualization(renderer_, effective, activeCam.view, activeCam.proj, cameraGizmoAspect);
		}
	}
}

void SceneBase::DrawEditorUiIfVisible() {
	if (EditorState::GetInstance().IsUiVisible()) {
		DrawImGui();
	}
}

void SceneBase::ProcessSceneTransitionRequest() {
	// シーン遷移条件は派生クラスごとに異なるため、ここでフックへ委譲する。
	// ImGuiのテキスト入力欄等がキーボードを掴んでいる間はEnter/Escape等のホットキーを
	// 無視する（Inspectorの名前欄でEnterを押しただけでシーン遷移してしまう等を防ぐ）
	if (!ImGui::GetIO().WantCaptureKeyboard) {
		HandleSceneTransitionInput();
	}

	// エディタUI表示中のみ、シーン遷移前に保存確認を挟む（Releaseビルド・F11で隠した実プレイ中は
	// 従来通り即座に遷移させる。保存確認はエディタ機能であって最終的なゲームプレイには不要なため）。
	// HandleSceneTransitionInputと"Objects"パネルの手動切替ボタン、どちらもnextScene_へ代入する
	// だけなので、ここで一箇所だけ横取りすれば両方をカバーできる
	if (EditorState::GetInstance().IsUiVisible()) {
		if (!nextScene_.empty() && !showTransitionSavePrompt_) {
			pendingTransitionRequest_ = nextScene_;
			nextScene_.clear(); // 確認が済むまでSceneManagerには見せない
			showTransitionSavePrompt_ = true;
			ImGui::OpenPopup("シーン遷移の確認##SceneTransitionSavePrompt");
		}
		if (showTransitionSavePrompt_) {
			DrawTransitionSavePrompt();
		}
	}
}

void SceneBase::DrawTransitionSavePrompt() {
	// OpenPopupは要求が発生した最初のフレームだけ呼べばよいが、BeginPopupModalは
	// 表示され続ける間、毎フレーム呼ぶ必要がある（ImGuiの標準的なモーダルの使い方）
	if (ImGui::BeginPopupModal("シーン遷移の確認##SceneTransitionSavePrompt", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("現在のシーンを保存しますか？");
		ImGui::Separator();
		if (ImGui::Button("保存して進む", ImVec2(140, 0))) {
			SaveScene();
			nextScene_ = pendingTransitionRequest_;
			showTransitionSavePrompt_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("保存せず進む", ImVec2(140, 0))) {
			nextScene_ = pendingTransitionRequest_;
			showTransitionSavePrompt_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", ImVec2(100, 0))) {
			pendingTransitionRequest_.clear();
			showTransitionSavePrompt_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
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

	ComponentLoadContext ctx = MakeComponentLoadContext();

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

	// RenderComponentBase系（Cube/Sphere/Triangle/Model/Sprite Render）は、TextureSelector/Mirrorの
	// 依存解決がGetComponent<RenderComponentBase>()の「最初の1個」だけを見る前提のため、
	// 1GameObjectに2個目を追加できてしまうと後から追加した方が無言で無視される
	// （テクスチャが反映されない、Transformの解釈がis3D/is2Dの食い違いで壊れる等）。
	// ここで既に1個持っていたら「モデル描画」「スプライト描画」の追加自体をブロックする
	bool alreadyHasRenderComponent = selected.GetComponent<RenderComponentBase>() != nullptr;

	DrawAddModelRenderNode(selected, ctx, alreadyHasRenderComponent);
	DrawAddSpriteRenderNode(selected, ctx, alreadyHasRenderComponent);
	DrawAddTextSpriteNode(selected, ctx);
	DrawAddTextureSelectorNode(selected, ctx);
	DrawAddMirrorNode(selected, ctx);
	DrawAddReflexEnemyHealthBarNode(selected, ctx);

	// AlphabetTextComponent：RenderComponentBase派生ではない（GameObject本体は描画を持たず、
	// SceneBase::RebuildAlphabetTextChildrenが生成する子GameObject側がModelRenderComponentで
	// 描画する）ため、Model/Spriteとの排他チェック対象外。既定値のまま追加してよい単純な型だが、
	// GetInstantAddCategories()の自動一覧には出さず（"描画"見出しの重複を避けるため）、
	// ここに個別のSelectableとして置く
	if (!selected.GetComponent<AlphabetTextComponent>()) {
		if (ImGui::Selectable("アルファベット文字列")) {
			ComponentRegistry::Create("AlphabetText", selected, ctx, nlohmann::json::object());
			ImGui::CloseCurrentPopup();
		}
	}

	// TextGroupComponent：AlphabetTextComponentと同じ理由でRenderComponentBase派生ではない
	// （GameObject本体は描画を持たず、SceneBase::RebuildTextGroupChildrenが生成する子GameObject側が
	// TextSpriteComponentで描画する）ため、Model/Spriteとの排他チェック対象外。自動一覧には出さず、
	// ここに個別のSelectableとして置く
	if (!selected.GetComponent<TextGroupComponent>()) {
		if (ImGui::Selectable("テキストグループ")) {
			ComponentRegistry::Create("TextGroup", selected, ctx, nlohmann::json::object());
			ImGui::CloseCurrentPopup();
		}
	}

	// ---- UI ----
	ImGui::SeparatorText("UI");

	DrawAddPlayButtonNode(selected, ctx);

	// ---- オーディオ ----
	ImGui::SeparatorText("オーディオ");

	DrawAddAudioSourceNode(selected, ctx);
	DrawAddHitSoundNode(selected, ctx);
	DrawAddSpawnSoundNode(selected, ctx);

	ImGui::EndPopup();
}

void SceneBase::DrawAddModelRenderNode(GameObject& selected, const ComponentLoadContext& ctx, bool alreadyHasRenderComponent) {
	// ModelRender：directoryPath/filenameを指定してLoadModelを呼び直す必要がある
	if (ImGui::TreeNode("モデル描画")) {
		static char modelDirBuf[256] = "Resources";
		static char modelFileBuf[256] = "";
		static bool modelHasAnimation = false;
		if (alreadyHasRenderComponent) {
			ImGui::TextDisabled("(既に描画コンポーネントが付いています。先に既存のものを削除してください)");
		}
		ImGui::InputText("ディレクトリ", modelDirBuf, sizeof(modelDirBuf));
		ImGui::InputText("ファイル名", modelFileBuf, sizeof(modelFileBuf));
		ImGui::Checkbox("アニメーションあり", &modelHasAnimation);
		bool canAdd = modelFileBuf[0] != '\0' && !alreadyHasRenderComponent;
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
}

void SceneBase::DrawAddSpriteRenderNode(GameObject& selected, const ComponentLoadContext& ctx, bool alreadyHasRenderComponent) {
	// SpriteRender：is3Dのみコンストラクタ引数。テクスチャ自体はTextureSelectorComponentを
	// 別途追加して選ぶ運用（RenderComponentBase::textureHandleは初期値kTextureNoneのまま）。
	// is3D/TransformComponent::is2Dは常に逆の関係になるよう両方向で同期する（is3D=falseなら
	// is2D=true、is3D=trueならis2D=false。片方向だけ更新すると、既にis2D=trueだったオブジェクトへ
	// 3D扱いで追加した場合にis3DとTransformの座標系が食い違い、3D空間にpx単位の座標がそのまま
	// 使われて描画がおかしくなる不具合が過去に発生したため）
	if (ImGui::TreeNode("スプライト描画")) {
		static bool spriteIs3D = true;
		if (alreadyHasRenderComponent) {
			ImGui::TextDisabled("(既に描画コンポーネントが付いています。先に既存のものを削除してください)");
		}
		ImGui::Checkbox("3D", &spriteIs3D);
		if (alreadyHasRenderComponent) ImGui::BeginDisabled();
		if (ImGui::Button("追加##SpriteRender")) {
			nlohmann::json data;
			data["is3D"] = spriteIs3D;
			ComponentRegistry::Create("SpriteRender", selected, ctx, data);
			TransformComponent* transform = selected.GetComponent<TransformComponent>();
			transform->is2D = !spriteIs3D;
			if (!spriteIs3D) {
				transform->translationSpeed = 1.0f; transform->translationMin = 0.0f; transform->translationMax = 1920.0f;
				transform->scaleSpeed = 1.0f; transform->scaleMin = 1.0f; transform->scaleMax = 1920.0f;
			}
			RebuildDerivedLists(); // is2Dが変わったのでgizmoTargets_/screenTargets_に反映させる
			ImGui::CloseCurrentPopup();
		}
		if (alreadyHasRenderComponent) ImGui::EndDisabled();
		ImGui::TreePop();
	}
}

void SceneBase::DrawAddTextSpriteNode(GameObject& selected, const ComponentLoadContext& ctx) {
	// TextSprite：常にスクリーン空間UI（is2D=true）専用（TextRenderComponentのような3D配置
	// オプションは持たない。詳しくはTextSpriteComponent.hのコメント参照）。
	// 他の描画コンポーネントとの排他ガードはユーザー指示により撤廃済み（同一GameObjectに
	// 複数のRenderComponentBase系を付けられる。TextureSelector/Mirror等、
	// GetComponent<RenderComponentBase>()で「最初の1個」だけを見る仕組みと組み合わせる場合は
	// 意図した方が先頭に来るよう追加順に注意すること）
	if (ImGui::TreeNode("テキストスプライト")) {
		if (ImGui::Button("追加##TextSprite")) {
			ComponentRegistry::Create("TextSprite", selected, ctx, nlohmann::json::object());
			TransformComponent* transform = selected.GetComponent<TransformComponent>();
			transform->is2D = true;
			transform->translationSpeed = 1.0f; transform->translationMin = 0.0f; transform->translationMax = 1920.0f;
			transform->scaleSpeed = 1.0f; transform->scaleMin = 1.0f; transform->scaleMax = 1920.0f;
			RebuildDerivedLists(); // is2Dが変わったのでgizmoTargets_/screenTargets_に反映させる
			ImGui::CloseCurrentPopup();
		}
		ImGui::TreePop();
	}
}

void SceneBase::DrawAddTextureSelectorNode(GameObject& selected, const ComponentLoadContext& ctx) {
	// TextureSelector：同じGameObjectに既にRenderComponentBase系（CubeRender/SphereRender/
	// SpriteRender等）が付いていることが前提。無ければ追加できないようにする。
	// Modelはサブメッシュごとのテクスチャ選択をModelRenderComponent自身のInspectorに内蔵した
	// ため、TextureSelectorComponentは対象外にする（付けても何にも使われず紛らわしいだけのため）
	if (ImGui::TreeNode("テクスチャ選択")) {
		bool hasModelRender = selected.GetComponent<ModelRenderComponent>() != nullptr;
		bool hasRenderComponent = selected.GetComponent<RenderComponentBase>() != nullptr;
		if (hasModelRender) {
			ImGui::TextDisabled("(モデルはInspectorのサブメッシュ別テクスチャで選択してください)");
		} else if (!hasRenderComponent) {
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
}

void SceneBase::DrawAddMirrorNode(GameObject& selected, const ComponentLoadContext& ctx) {
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
}

void SceneBase::DrawAddReflexEnemyHealthBarNode(GameObject& selected, const ComponentLoadContext& ctx) {
	// ReflexEnemyHealthBar：兄弟のReflexEnemyComponentからhp/maxHpを読んでバーを描く。
	// ReflexEnemyComponentが無いGameObjectに付けた場合はtarget_がnullptrになり、
	// ReflexEnemyHealthBarComponent::Updateの先頭ガードにより何も描画されないだけで安全
	if (ImGui::TreeNode("REFLEX敵HPバー")) {
		if (ImGui::Button("追加##ReflexEnemyHealthBar")) {
			ComponentRegistry::Create("ReflexEnemyHealthBar", selected, ctx, nlohmann::json::object());
			ImGui::CloseCurrentPopup();
		}
		ImGui::TreePop();
	}
}

void SceneBase::DrawAddAudioSourceNode(GameObject& selected, const ComponentLoadContext& ctx) {
	// AudioSource：filePath/registeredName/type/loopを指定してSound::Loadを呼び直す必要がある
	if (ImGui::TreeNode("オーディオソース")) {
		static char audioPathBuf[256] = "";
		static char audioNameBuf[128] = "";
		static int  audioTypeIndex = 0; // 0=BGM, 1=SE
		static bool audioLoop = true;
		static bool audioPlayOnAwake = true; // Unity同様、生成直後に自動再生するか
		static float audioVolume = 1.0f;
		const char* typeNames[] = { "BGM", "SE" };
		ImGui::InputText("ファイルパス", audioPathBuf, sizeof(audioPathBuf));
		ImGui::InputText("登録名", audioNameBuf, sizeof(audioNameBuf));
		ImGui::Combo("サウンド種別", &audioTypeIndex, typeNames, 2);
		ImGui::Checkbox("ループ", &audioLoop);
		ImGui::Checkbox("開始時に自動再生 (Play On Awake)", &audioPlayOnAwake);
		ImGui::SliderFloat("音量", &audioVolume, 0.0f, 1.0f);
		bool canAdd = audioPathBuf[0] != '\0' && audioNameBuf[0] != '\0';
		if (!canAdd) ImGui::BeginDisabled();
		if (ImGui::Button("追加##AudioSource")) {
			nlohmann::json data;
			data["filePath"] = std::string(audioPathBuf);
			data["registeredName"] = std::string(audioNameBuf);
			data["soundType"] = audioTypeIndex;
			data["loop"] = audioLoop;
			data["playOnAwake"] = audioPlayOnAwake;
			data["volume"] = audioVolume;
			ComponentRegistry::Create("AudioSource", selected, ctx, data);
			audioPathBuf[0] = '\0';
			audioNameBuf[0] = '\0';
			ImGui::CloseCurrentPopup();
		}
		if (!canAdd) ImGui::EndDisabled();
		ImGui::TreePop();
	}
}

void SceneBase::DrawAddHitSoundNode(GameObject& selected, const ComponentLoadContext& ctx) {
	// HitSound：TextureSelectorと同じ「プロジェクトパネルの一覧からコンボで選ぶ」方式。
	// パス入力・D&Dは行わず、projectAudioClips_（Resources/配下走査済みの音声一覧）から選ぶだけ
	if (ImGui::TreeNode("ヒットSE")) {
		static int  hitSoundIndex = 0;
		static float hitSoundVolume = 1.0f;
		if (projectAudioClips_.empty()) {
			ImGui::TextDisabled("(利用可能な音声がありません)");
		} else {
			if (hitSoundIndex >= static_cast<int>(projectAudioClips_.size())) hitSoundIndex = 0;
			if (ImGui::BeginCombo("SE", projectAudioClips_[hitSoundIndex].displayName.c_str())) {
				for (int i = 0; i < static_cast<int>(projectAudioClips_.size()); i++) {
					bool isSelected = (i == hitSoundIndex);
					if (ImGui::Selectable(projectAudioClips_[i].displayName.c_str(), isSelected)) hitSoundIndex = i;
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SliderFloat("音量", &hitSoundVolume, 0.0f, 1.0f);
			if (ImGui::Button("追加##HitSound")) {
				nlohmann::json data;
				data["audioName"] = projectAudioClips_[hitSoundIndex].displayName;
				data["volume"] = hitSoundVolume;
				ComponentRegistry::Create("HitSound", selected, ctx, data);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::TreePop();
	}
}

void SceneBase::DrawAddSpawnSoundNode(GameObject& selected, const ComponentLoadContext& ctx) {
	// SpawnSound：DrawAddHitSoundNodeと完全に同じ構造（敵がスポーンした瞬間に鳴らすSE）
	if (ImGui::TreeNode("スポーンSE")) {
		static int  spawnSoundIndex = 0;
		static float spawnSoundVolume = 1.0f;
		if (projectAudioClips_.empty()) {
			ImGui::TextDisabled("(利用可能な音声がありません)");
		} else {
			if (spawnSoundIndex >= static_cast<int>(projectAudioClips_.size())) spawnSoundIndex = 0;
			if (ImGui::BeginCombo("SE##SpawnSound", projectAudioClips_[spawnSoundIndex].displayName.c_str())) {
				for (int i = 0; i < static_cast<int>(projectAudioClips_.size()); i++) {
					bool isSelected = (i == spawnSoundIndex);
					if (ImGui::Selectable(projectAudioClips_[i].displayName.c_str(), isSelected)) spawnSoundIndex = i;
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::SliderFloat("音量##SpawnSound", &spawnSoundVolume, 0.0f, 1.0f);
			if (ImGui::Button("追加##SpawnSound")) {
				nlohmann::json data;
				data["audioName"] = projectAudioClips_[spawnSoundIndex].displayName;
				data["volume"] = spawnSoundVolume;
				ComponentRegistry::Create("SpawnSound", selected, ctx, data);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::TreePop();
	}
}

void SceneBase::DrawAddPlayButtonNode(GameObject& selected, const ComponentLoadContext& ctx) {
	// PlayButton：TitleScene用のPLAYボタン。クリック音はHitSound/SpawnSoundと違い、
	// 未設定（(なし)選択）のままでも追加できる（音声ファイルが後日追加される想定のため）
	if (ImGui::TreeNode("PLAYボタン")) {
		static int  playButtonSoundIndex = -1; // -1 = 未設定
		static float playButtonVolume = 1.0f;
		std::string currentName = (playButtonSoundIndex >= 0 && playButtonSoundIndex < static_cast<int>(projectAudioClips_.size()))
			? projectAudioClips_[playButtonSoundIndex].displayName : "(未設定)";
		if (ImGui::BeginCombo("クリックSE##PlayButton", currentName.c_str())) {
			bool noneSelected = (playButtonSoundIndex < 0);
			if (ImGui::Selectable("(未設定)", noneSelected)) playButtonSoundIndex = -1;
			if (noneSelected) ImGui::SetItemDefaultFocus();
			for (int i = 0; i < static_cast<int>(projectAudioClips_.size()); i++) {
				bool isSelected = (i == playButtonSoundIndex);
				if (ImGui::Selectable(projectAudioClips_[i].displayName.c_str(), isSelected)) playButtonSoundIndex = i;
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SliderFloat("音量##PlayButton", &playButtonVolume, 0.0f, 1.0f);
		if (ImGui::Button("追加##PlayButton")) {
			nlohmann::json data;
			data["audioName"] = (playButtonSoundIndex >= 0) ? projectAudioClips_[playButtonSoundIndex].displayName : std::string();
			data["volume"] = playButtonVolume;
			ComponentRegistry::Create("PlayButton", selected, ctx, data);
			ImGui::CloseCurrentPopup();
		}
		ImGui::TreePop();
	}
}

void SceneBase::DrawImGui() {
	ImGui::Begin("ギズモ##Gizmo");

	DrawPlayStopControls();
	DrawSceneGameViewToggle();

	// Edit Collider/操作モードはGizmoControllerが描画（オブジェクト選択自体はHierarchyパネルへ一本化済み）
	gizmoController_.DrawImGui(gizmoTargets_);

	if (ImGui::Button("選択を削除")) DeleteSelectedObject();

	ImGui::Separator();
	DrawSceneSaveLoadControls();

	ImGui::Separator();
	DrawSceneTransitionButtons();

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

void SceneBase::DrawPlayStopControls() {
	// Stop中はGravityComponent等の更新と押し戻しを止める
	if (isPlaying_) {
		if (ImGui::Button("停止")) isPlaying_ = false;
	} else {
		if (ImGui::Button("再生")) isPlaying_ = true;
	}
	ImGui::SameLine();
	ImGui::Text(isPlaying_ ? "（再生中）" : "（停止中）");
}

void SceneBase::DrawSceneGameViewToggle() {
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
}

void SceneBase::DrawSceneSaveLoadControls() {
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
}

void SceneBase::DrawSceneTransitionButtons() {
	// シーン遷移。今まではキーボード（Enter/Escape/F1等）のみだったため、ImGuiからも
	// 直接切り替えられるようにする。ボタンはnextScene_へ代入するだけで、実際の切替は
	// 既存のSceneManager::Render()内（GetNextScene()を見てChangeScene）で行われる
	ImGui::Text("シーン切替");
	// SceneRegistryに登録済みの全シーン名を動的に列挙してボタン化する（REGISTER_SCENEで
	// 新しいシーンを追加するだけで、ここを編集しなくても切替ボタンが増える）
	bool firstSceneButton = true;
	for (const std::string& sceneName : SceneRegistry::GetAllNames()) {
		if (!firstSceneButton) ImGui::SameLine();
		firstSceneButton = false;
		if (ImGui::Button(sceneName.c_str())) nextScene_ = sceneName;
	}
}

// 全オブジェクトを名前クリックで選べる一覧パネル。選択状態の実体はGizmoControllerの
// インデックスベース管理のまま変えず、「クリックされたオブジェクトのインデックスを引いて
// 選択状態にセットする」GizmoController::SetSelected/SetSelected2Dを呼ぶだけに留める
// （既存のGizmoウィンドウ内Targetコンボと選択状態を共有するため、片方で選んでももう片方に反映される）
// ドラッグ&ドロップで親子付けするときのペイロード種別名（BeginDragDropSource/Targetで対応させる）
static const char* kHierarchyDragDropId = "HIERARCHY_GAMEOBJECT";

// プロジェクトパネル→ヒエラルキー/インスペクターへのドラッグ&ドロップ用ペイロードID。
// SceneBaseのメンバprojectImages_/projectAudioClips_/projectModels_内のstd::stringを指すポインタを運ぶ
// （ドラッグ中に再構築されない限りアドレスが安定しているため）。
// kProjectImageDragDropIdだけはModelRenderComponent.cppからも使うためTextureEntry.hで共有定義している
static const char* kProjectAudioDragDropId     = "PROJECT_AUDIO_ASSET";
static const char* kProjectModelDragDropId     = "PROJECT_MODEL_ASSET";

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
		// Floorのような極端に平たい/巨大なオブジェクトはBounding Sphereでのレイキャスト判定が
		// 実際の見た目よりはるかに巨大になり、Sceneビュー上のどこをクリックしてもこのオブジェクトが
		// 最優先でヒットしてしまい「何もない空間をクリックしても選択解除できない」原因になる。
		// ONにするとクリックでの選択・選択解除の判定対象から外れる（コンボボックス等からの選択は影響しない）
		ImGui::Checkbox("クリック選択の対象外にする（Floor等の平たい形状向け）", &selected->excludeFromPicking);
		ImGui::Separator();

		selected->DrawImGui();

		DrawSceneSpecificInspectorExtensions(*selected);

		ImGui::Separator();
		DrawAddComponentMenu(*selected);
	} else {
		ImGui::TextDisabled("(オブジェクト未選択)");
	}

	// プロジェクトパネルからのドロップ受け入れ：残りの余白へ落とすと選択中オブジェクトへ
	// 画像/音声を付与する。BeginDragDropTargetは直前に何か「アイテム」が要るため、
	// ヒエラルキーの余白ドロップ（##hierarchy_root_drop）と同じくInvisibleButtonを土台にする
	// （幅・高さのどちらかが0だとIM_ASSERTで落ちる既知のクラッシュパターンのため最低1pxを保証）
	ImVec2 dropSize = ImGui::GetContentRegionAvail();
	if (dropSize.x < 1.0f) dropSize.x = 1.0f;
	if (dropSize.y < 1.0f) dropSize.y = 1.0f;
	ImGui::InvisibleButton("##inspector_asset_drop", dropSize);
	if (ImGui::BeginDragDropTarget()) {
		if (selected) {
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kProjectImageDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(p->Data));
				AttachTextureAsset(*selected, path);
			}
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kProjectAudioDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(p->Data));
				AttachAudioAsset(*selected, path);
			}
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kProjectModelDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(p->Data));
				AttachModelAsset(*selected, path);
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::End();
}

// Unityの「Projectビュー」相当。ユーザーが追加した画像・音声をカテゴリごとの
// アイコングリッドとして表示する。アイコンをドラッグして
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

	// 画像：Resources/配下から見つかった画像ファイルを実テクスチャのサムネイルで表示する
	auto drawImageIcon = [this](const ProjectAssetEntry& asset, float iconSize) {
		// LoadTextureは同じパスを二重ロードしない（TextureManagerがパスでキャッシュする）ため、
		// 毎フレーム呼んでもコストは無視できる
		TextureHandle handle = renderer_->LoadTexture(asset.path);
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = renderer_->GetTextureSrvGpuHandle(handle);
		ImGui::Image((ImTextureID)gpu.ptr, ImVec2(iconSize, iconSize));
	};
	ImGui::SeparatorText("画像");
	DrawProjectAssetGrid(projectImages_, kProjectImageDragDropId, kIconSize, kTileWidth, columnCount, drawImageIcon);

	// 音声：波形サムネイル等は無いため、種別を示す固定色のアイコンにする
	auto drawAudioIcon = [kIconFlags](const ProjectAssetEntry&, float iconSize) {
		ImGui::ColorButton("##icon", ImVec4(0.35f, 0.55f, 0.9f, 1.0f), kIconFlags, ImVec2(iconSize, iconSize));
	};
	ImGui::SeparatorText("音声");
	DrawProjectAssetGrid(projectAudioClips_, kProjectAudioDragDropId, kIconSize, kTileWidth, columnCount, drawAudioIcon);

	// モデル：サムネイル描画（オフスクリーンレンダリング）は無いため、音声と同じく
	// 種別を示す固定色のアイコンにする。「まだ描画コンポーネントが付いていないGameObjectに
	// ドラッグすると新規にModelRenderComponentが付く」点だけ画像/音声と挙動が異なる
	// （AttachModelAsset参照）
	auto drawModelIcon = [kIconFlags](const ProjectAssetEntry&, float iconSize) {
		ImGui::ColorButton("##icon", ImVec4(0.85f, 0.55f, 0.25f, 1.0f), kIconFlags, ImVec2(iconSize, iconSize));
	};
	ImGui::SeparatorText("モデル");
	DrawProjectAssetGrid(projectModels_, kProjectModelDragDropId, kIconSize, kTileWidth, columnCount, drawModelIcon);

	ImGui::End();
}

void SceneBase::DrawProjectAssetGrid(
	const std::vector<ProjectAssetEntry>& assets, const char* dragDropId,
	float iconSize, float tileWidth, int columnCount,
	const std::function<void(const ProjectAssetEntry&, float iconSize)>& drawIcon) {
	int col = 0;
	for (const auto& asset : assets) {
		ImGui::PushID(asset.path.c_str());
		ImGui::BeginGroup();
		drawIcon(asset, iconSize);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tileWidth - 4.0f);
		ImGui::TextWrapped("%s", asset.displayName.c_str());
		ImGui::PopTextWrapPos();
		ImGui::EndGroup();
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			const std::string* pathPtr = &asset.path;
			ImGui::SetDragDropPayload(dragDropId, &pathPtr, sizeof(const std::string*));
			ImGui::Text("%s", asset.displayName.c_str());
			ImGui::EndDragDropSource();
		}
		ImGui::PopID();
		col++;
		if (col < columnCount) ImGui::SameLine(0.0f, 12.0f);
		else col = 0;
	}
	// 最後の行が列を埋めきらなかった場合、直前のSameLine()でカーソルがまだ同じ行に
	// 残っている。そのままだと次のSeparatorTextがこの行の続きに描かれてしまうため改行する
	if (col != 0) ImGui::NewLine();
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

// DrawHierarchyの木構造描画（旧drawNode/drawInsertionGapラムダ）を切り出したヘルパー。
// 1回のDrawHierarchy呼び出しの間だけ生存する一時状態（選択中オブジェクトcurrent_、
// 右クリック削除の保留先pendingDelete_）をメンバに持つ。drawNodeの自己再帰は
// std::functionを介さず、DrawNode()からDrawNode()を直接呼ぶ形にする（挙動は同一）
class SceneBase::HierarchyTreeDrawer {
public:
	HierarchyTreeDrawer(SceneBase* scene, GameObject* current) : scene_(scene), current_(current) {}

	// 兄弟同士の「境目」に挟む薄い透明なドロップターゲット。ここへドロップすると、
	// onDropが指定した位置への挿入（並べ替え・別の親への移動）になる。ノード本体へのドロップ
	// （DrawNode内の既存のBeginDragDropTarget）は「子として末尾に追加」のまま変更しない
	void DrawInsertionGap(const std::function<void(GameObject*)>& onDrop) {
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
				scene_->RebuildDerivedLists();
			}
			ImGui::EndDragDropTarget();
		}
	}

	// 1オブジェクト分のノードを描画し、GetChildren()を再帰的に描画する。
	// 選択はis2Dで実際の所属を見て正しいSetSelected/SetSelected2Dへ振り分ける
	// （どちらのセクションから辿り着いたかに関係なく、子は親と異なるis2Dを持つ可能性があるため）
	void DrawNode(GameObject* obj) {
		ImGui::PushID(obj);
		// hasChildrenはこのノードを開く前に1回だけ確定する。ドロップ処理（下のBeginDragDropTarget）
		// でこのノードの子が増減する可能性があり、TreeNodeExへ渡したフラグ（Leaf/NoTreePushOnOpen）と
		// 後段のTreePop要否判定が食い違うとPushID/TreePopの対応が崩れてImGuiがクラッシュするため、
		// 同じ値を最後まで使い回す
		bool hasChildren = !obj->GetChildren().empty();
		// DefaultOpenは付けない：起動直後は全ての親ノード（"Enemies"/"Particles"フォルダ等）が
		// 閉じた状態から始まる。ユーザーが手動で開閉した状態はImGuiが内部的（ID基準）に覚えているため、
		// 一度開けばそれ以降はセッション中閉じるまで開いたままになる
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (obj == current_) flags |= ImGuiTreeNodeFlags_Selected;

		bool opened = ImGui::TreeNodeEx(obj->name.c_str(), flags);
		if (ImGui::IsItemClicked()) {
			bool is2D = obj->GetComponent<TransformComponent>()->is2D;
			if (is2D) scene_->gizmoController_.SetSelected2D(obj, scene_->screenTargets_);
			else      scene_->gizmoController_.SetSelected(obj, scene_->gizmoTargets_);
		}

		// 右クリックでこのノード専用の削除メニューをマウスカーソル付近に出す（Unityの
		// 「Hierarchyで右クリック→Delete」相当）。BeginPopupContextItemは直前のアイテム
		// （このTreeNodeEx）に紐付き、右クリックで自動的にカーソル位置へ開く
		if (ImGui::BeginPopupContextItem("HierarchyNodeContext")) {
			// 右クリックした時点でこのオブジェクトを選択状態にし、操作対象をInspector等でも明確にする
			bool is2D = obj->GetComponent<TransformComponent>()->is2D;
			if (is2D) scene_->gizmoController_.SetSelected2D(obj, scene_->screenTargets_);
			else      scene_->gizmoController_.SetSelected(obj, scene_->gizmoTargets_);

			if (ImGui::MenuItem("削除")) {
				pendingDelete_ = obj;
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
				scene_->RebuildDerivedLists();
			}
			// プロジェクトパネルからのドロップ：このオブジェクトへ画像/音声を付与する
			// （Unityの「アセットをHierarchyのオブジェクトへドラッグ」に相当）
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectImageDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(payload->Data));
				scene_->AttachTextureAsset(*obj, path);
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAudioDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(payload->Data));
				scene_->AttachAudioAsset(*obj, path);
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectModelDragDropId)) {
				const std::string& path = *(*static_cast<const std::string* const*>(payload->Data));
				scene_->AttachModelAsset(*obj, path);
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
				DrawInsertionGap([&, i](GameObject* dropped) { obj->ReparentAt(dropped, i); });
				ImGui::PopID();
				DrawNode(childrenSnapshot[i]);
			}
			// 子リストの末尾にも1つ置く（末尾へ挿入するため）
			ImGui::PushID(static_cast<int>(childrenSnapshot.size()));
			DrawInsertionGap([&, count = childrenSnapshot.size()](GameObject* dropped) { obj->ReparentAt(dropped, count); });
			ImGui::PopID();
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// 木構造の描画がすべて終わった後、右クリック削除の対象を取り出す（無ければnullptr）
	GameObject* TakePendingDelete() const { return pendingDelete_; }

private:
	SceneBase* scene_;
	GameObject* current_;
	// 右クリックメニューで「削除」が押された対象。DrawNode（再帰中）でその場でobjects_.erase()すると、
	// 削除したobjを使い続けている呼び出し元スタック（hasChildren判定・子の再帰描画・TreePop等）が
	// 解放済みポインタに触れてしまうため、木構造の描画が全部終わってから実際に削除する
	GameObject* pendingDelete_ = nullptr;
};

void SceneBase::DrawHierarchy() {
	ImGui::Begin("ヒエラルキー##Hierarchy");

	GameObject* current = gizmoController_.GetSelectedPreferLatest(gizmoTargets_, screenTargets_);
	HierarchyTreeDrawer treeDrawer(this, current);

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
		treeDrawer.DrawInsertionGap([&, rootVisibleIdx](GameObject* dropped) { ReorderRootObject(dropped, rootVisibleIdx); });
		ImGui::PopID();
		ImGui::PopID();
		treeDrawer.DrawNode(obj);
		rootVisibleIdx++;
	}
	// ルート直下リストの末尾にも1つ置く（末尾へ挿入するため）
	ImGui::PushID(static_cast<int>(rootVisibleIdx));
	ImGui::PushID("RootGap");
	treeDrawer.DrawInsertionGap([&, rootVisibleIdx](GameObject* dropped) { ReorderRootObject(dropped, rootVisibleIdx); });
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

	// 何もない場所を右クリックすると新規作成メニューを出す（Unityの「Hierarchyで右クリック→Create」
	// 相当）。BeginPopupContextItemは直前のアイテム（##hierarchy_root_dropのInvisibleButton）に紐付く。
	// ObjectArchetypes（キューブ/球/光源/カメラ等の完成形ひな形）は既存のJSON復元パイプラインに
	// そのまま流し込める形のJSONを返す設計で用意されていたが、これまでどこからも呼ばれていなかった
	// ため、ここで初めて実際に使う
	if (ImGui::BeginPopupContextItem("HierarchyCreateContext")) {
		if (ImGui::MenuItem("空のオブジェクト")) {
			CreateObject("新規追加 " + std::to_string(objects_.size() + 1));
			RebuildDerivedLists();
		}
		ImGui::Separator();
		ComponentLoadContext ctx = MakeComponentLoadContext();
		for (const auto& archetypeName : ObjectArchetypes::GetNames()) {
			if (ImGui::MenuItem(archetypeName.c_str())) {
				GameObject& obj = CreateObject(archetypeName);
				obj.FromJson(ObjectArchetypes::GetJson(archetypeName), ctx);
				RebuildDerivedLists();
			}
		}
		ImGui::EndPopup();
	}

	// 右クリックメニューで「削除」が押されていれば、木構造の描画が全部終わった今ここで実際に消す
	if (GameObject* pendingDelete = treeDrawer.TakePendingDelete()) {
		DeleteObjects({ pendingDelete });
	}

	ImGui::End();
}
