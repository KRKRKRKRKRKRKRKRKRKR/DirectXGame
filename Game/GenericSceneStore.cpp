#include "GenericSceneStore.h"
#include "SceneRegistry.h"
#include "SceneBase.h"
#include "../Externals/Json/json.hpp"
#include "../Engine/Utils/Logger.h"
#include <fstream>
#include <filesystem>
#include <memory>
#include <cctype>

namespace {
	constexpr const char* kScenesJsonPath = "Resources/scenes.json";
	constexpr const char* kDefaultSceneName = "Main";

	// CreateNewScript(SceneBase.cpp)のIsValidScriptBaseNameと同じ考え方：英字/_で始まり、
	// 英数字と_のみ。シーン名はそのままResources/{name}/というフォルダ名にもなるため、
	// Windowsのパスとして安全な文字種に絞る
	bool IsValidSceneName(const std::string& name) {
		if (name.empty()) return false;
		if (!(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) return false;
		for (char c : name) {
			if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
		}
		return true;
	}

	nlohmann::json LoadScenesJson() {
		std::ifstream file(kScenesJsonPath);
		if (!file) return nlohmann::json::object();
		nlohmann::json root = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
		if (root.is_discarded()) {
			Logger::Log(std::string("GenericSceneStore: failed to parse ") + kScenesJsonPath + " (JSON破損)\n");
			return nlohmann::json::object();
		}
		return root;
	}

	void SaveScenesJson(const nlohmann::json& root) {
		std::filesystem::create_directories("Resources");
		std::ofstream file(kScenesJsonPath);
		if (!file) {
			Logger::Log(std::string("GenericSceneStore: failed to open ") + kScenesJsonPath + " for writing\n");
			return;
		}
		file << root.dump(4);
	}

	std::unique_ptr<IScene> MakeGenericScene() {
		return std::make_unique<SceneBase>();
	}
}

std::string GenericSceneStore::LoadOrCreateDefault() {
	nlohmann::json root = LoadScenesJson();

	if (!root.contains("scenes") || !root["scenes"].is_array() || root["scenes"].empty()) {
		// scenes.jsonが無い/空＝このリセット後の初回起動。エディタUI(DrawSceneTransitionButtons)は
		// アクティブなシーンが無いと描画できず「+」ボタンにも辿り着けないため、最低1つの
		// デフォルトシーンを自動生成しておく
		root["scenes"] = { kDefaultSceneName };
		root["startScene"] = kDefaultSceneName;
		std::filesystem::create_directories(std::string("Resources/") + kDefaultSceneName);
		SaveScenesJson(root);
	}

	for (const auto& nameJson : root["scenes"]) {
		SceneRegistry::Register(nameJson.get<std::string>(), MakeGenericScene);
	}

	std::string startScene = root.value("startScene", std::string(kDefaultSceneName));
	// startSceneがscenes配列から消えていた（手動編集等）場合に備えて、登録済みの先頭へフォールバックする
	bool startSceneRegistered = false;
	for (const auto& nameJson : root["scenes"]) {
		if (nameJson.get<std::string>() == startScene) { startSceneRegistered = true; break; }
	}
	if (!startSceneRegistered && !root["scenes"].empty()) {
		startScene = root["scenes"].front().get<std::string>();
	}
	return startScene;
}

std::string GenericSceneStore::CreateScene(const std::string& name) {
	if (!IsValidSceneName(name)) {
		return "シーン名が不正です（英字/_で始まり、英数字と_のみ使えます）";
	}

	nlohmann::json root = LoadScenesJson();
	if (!root.contains("scenes") || !root["scenes"].is_array()) root["scenes"] = nlohmann::json::array();

	for (const auto& nameJson : root["scenes"]) {
		if (nameJson.get<std::string>() == name) return "同名のシーンが既に存在します";
	}

	SceneRegistry::Register(name, MakeGenericScene);
	std::filesystem::create_directories("Resources/" + name);

	root["scenes"].push_back(name);
	if (!root.contains("startScene")) root["startScene"] = name;
	SaveScenesJson(root);
	return "";
}

std::string GenericSceneStore::DeleteScene(const std::string& name) {
	nlohmann::json root = LoadScenesJson();
	if (!root.contains("scenes") || !root["scenes"].is_array()) {
		return "このシーンは削除できません（scenes.json管理下のシーンではありません）";
	}

	bool found = false;
	nlohmann::json remaining = nlohmann::json::array();
	for (const auto& nameJson : root["scenes"]) {
		std::string existing = nameJson.get<std::string>();
		if (existing == name) { found = true; continue; }
		remaining.push_back(existing);
	}
	if (!found) {
		return "このシーンは削除できません（scenes.json管理下のシーンではありません。REGISTER_SCENEで登録されたC++シーンはコード側で削除してください）";
	}
	if (remaining.empty()) {
		return "最後の1つのシーンは削除できません（エディタUIを開けなくなるため）";
	}

	root["scenes"] = remaining;
	if (root.value("startScene", std::string()) == name) {
		root["startScene"] = remaining.front().get<std::string>();
	}
	SaveScenesJson(root);

	SceneRegistry::Unregister(name);

	std::error_code ec;
	std::filesystem::remove_all("Resources/" + name, ec);
	if (ec) {
		Logger::Log("GenericSceneStore::DeleteScene: Resources/" + name + " の削除に失敗しました: " + ec.message() + "\n");
	}
	return "";
}
