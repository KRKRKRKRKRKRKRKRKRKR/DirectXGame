#include "SceneRegistry.h"
#include "GenericScene.h"
#include "../Engine/Utils/Logger.h"
#include "../Externals/Json/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {
// 「削除」された固定シーン名の永続リスト。REGISTER_SCENEは静的初期化で必ず実行されるため、
// C++コードそのものを消さない限りSceneRegistry::Unregisterだけでは再起動後に復活してしまう。
// このファイルに記録した名前をSceneRegistry::ApplyPermanentlyDeletedScenesが起動時に読み、
// もう一度Unregisterし直すことで「実行するたびに消す」を実現する
constexpr const char* kDeletedScenesPath = "Resources/DeletedScenes.json";

// シーン切替ボタンの並び順（GetAllNames()の順序）の永続化先。既定は登録順（REGISTER_SCENEの
// 静的初期化順＋動的登録順）だが、SceneBase::DrawSceneTransitionButtonsの「↑」「↓」ボタンで
// 並び替えるとSaveOrderがここへ書き出し、次回起動時にLoadOrderが読んで復元する
constexpr const char* kSceneOrderPath = "Resources/SceneOrder.json";
}

bool SceneRegistry::IsRegistered(const std::string& name) {
	return Factories().find(name) != Factories().end();
}

bool SceneRegistry::RegisterGenericIfMissing(const std::string& name) {
	if (IsRegistered(name)) return false;
	Register(name, []() { return std::make_unique<GenericScene>(); });
	GenericFlags()[name] = true;
	return true;
}

bool SceneRegistry::IsGeneric(const std::string& name) {
	auto it = GenericFlags().find(name);
	return it != GenericFlags().end() && it->second;
}

void SceneRegistry::Unregister(const std::string& name) {
	Factories().erase(name);
	GenericFlags().erase(name);
	auto& names = Names();
	names.erase(std::remove(names.begin(), names.end(), name), names.end());
}

bool SceneRegistry::Rename(const std::string& oldName, const std::string& newName) {
	if (!IsRegistered(oldName) || IsRegistered(newName)) return false;

	auto& factories = Factories();
	Factory factory = std::move(factories[oldName]);
	factories.erase(oldName);
	factories[newName] = std::move(factory);

	auto& genericFlags = GenericFlags();
	auto genericIt = genericFlags.find(oldName);
	if (genericIt != genericFlags.end()) {
		bool wasGeneric = genericIt->second;
		genericFlags.erase(genericIt);
		genericFlags[newName] = wasGeneric;
	}

	// 登録順（切替ボタンの並び順）を維持したまま、該当要素の文字列だけ差し替える
	for (std::string& n : Names()) {
		if (n == oldName) { n = newName; break; }
	}
	return true;
}

std::unordered_map<std::string, bool>& SceneRegistry::GenericFlags() {
	static std::unordered_map<std::string, bool> flags;
	return flags;
}

std::unordered_map<std::string, SceneRegistry::Factory>& SceneRegistry::Factories() {
	static std::unordered_map<std::string, Factory> factories;
	return factories;
}

std::vector<std::string>& SceneRegistry::Names() {
	static std::vector<std::string> names;
	return names;
}

void SceneRegistry::Register(const std::string& name, Factory factory) {
	Factories()[name] = std::move(factory);
	Names().push_back(name);
}

std::unique_ptr<IScene> SceneRegistry::Create(const std::string& name) {
	auto it = Factories().find(name);
	if (it == Factories().end()) return nullptr;
	return it->second();
}

const std::vector<std::string>& SceneRegistry::GetAllNames() {
	return Names();
}

void SceneRegistry::RecordPermanentlyDeleted(const std::string& name) {
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories("Resources", ec);

	nlohmann::json j;
	std::ifstream in(kDeletedScenesPath, std::ios::binary);
	if (in.is_open()) {
		try {
			in >> j;
		} catch (const std::exception&) {
			j = nlohmann::json::object();
		}
	}
	if (!j.contains("names") || !j["names"].is_array()) {
		j["names"] = nlohmann::json::array();
	}

	for (const auto& existing : j["names"]) {
		if (existing.is_string() && existing.get<std::string>() == name) return; // 既に記録済み
	}
	j["names"].push_back(name);

	std::ofstream out(kDeletedScenesPath, std::ios::binary | std::ios::trunc);
	if (out.is_open()) {
		out << j.dump(2);
	} else {
		Logger::Log("SceneRegistry::RecordPermanentlyDeleted: 書き込みに失敗しました '" + std::string(kDeletedScenesPath) + "'\n");
	}
}

void SceneRegistry::ApplyPermanentlyDeletedScenes() {
	std::ifstream in(kDeletedScenesPath, std::ios::binary);
	if (!in.is_open()) return;

	nlohmann::json j;
	try {
		in >> j;
	} catch (const std::exception& e) {
		Logger::Log("SceneRegistry::ApplyPermanentlyDeletedScenes: 解析に失敗しました: " + std::string(e.what()) + "\n");
		return;
	}
	if (!j.contains("names") || !j["names"].is_array()) return;

	for (const auto& entry : j["names"]) {
		if (!entry.is_string()) continue;
		Unregister(entry.get<std::string>());
	}
}

void SceneRegistry::MoveUp(size_t index) {
	auto& names = Names();
	if (index == 0 || index >= names.size()) return;
	std::swap(names[index - 1], names[index]);
}

void SceneRegistry::MoveDown(size_t index) {
	auto& names = Names();
	if (index + 1 >= names.size()) return;
	std::swap(names[index], names[index + 1]);
}

void SceneRegistry::SaveOrder() {
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories("Resources", ec);

	nlohmann::json j;
	j["names"] = Names();

	std::ofstream out(kSceneOrderPath, std::ios::binary | std::ios::trunc);
	if (out.is_open()) {
		out << j.dump(2);
	} else {
		Logger::Log("SceneRegistry::SaveOrder: 書き込みに失敗しました '" + std::string(kSceneOrderPath) + "'\n");
	}
}

void SceneRegistry::LoadOrder() {
	std::ifstream in(kSceneOrderPath, std::ios::binary);
	if (!in.is_open()) return;

	nlohmann::json j;
	try {
		in >> j;
	} catch (const std::exception& e) {
		Logger::Log("SceneRegistry::LoadOrder: 解析に失敗しました: " + std::string(e.what()) + "\n");
		return;
	}
	if (!j.contains("names") || !j["names"].is_array()) return;

	// ファイルに載っている順序のうち、現在も登録済みの名前だけを新しい並びとして採用する
	// （削除済み等で未登録の名前は無視）。ファイルに載っていない登録済みの名前
	// （並び順保存後に新規追加されたシーン等）は、元の並び順を保ったまま末尾に足す
	std::vector<std::string> ordered;
	for (const auto& entry : j["names"]) {
		if (!entry.is_string()) continue;
		std::string name = entry.get<std::string>();
		if (IsRegistered(name)) ordered.push_back(name);
	}
	for (const std::string& name : Names()) {
		bool alreadyPlaced = std::find(ordered.begin(), ordered.end(), name) != ordered.end();
		if (!alreadyPlaced) ordered.push_back(name);
	}
	Names() = std::move(ordered);
}

void SceneRegistry::ScanResourcesForGenericScenes() {
	namespace fs = std::filesystem;
	std::error_code ec;
	if (!fs::exists("Resources", ec) || ec) return;

	for (auto& entry : fs::directory_iterator("Resources", ec)) {
		if (ec || !entry.is_directory()) continue;
		std::string name = entry.path().filename().string();
		if (IsRegistered(name)) continue; // REGISTER_SCENE済みの固定シーンはスキップ

		std::error_code existsEc;
		bool hasSceneJson = fs::exists(entry.path() / "scene.json", existsEc);
		bool hasUiJson = fs::exists(entry.path() / "ui.json", existsEc);
		if (hasSceneJson || hasUiJson) {
			RegisterGenericIfMissing(name);
		}
	}
}
