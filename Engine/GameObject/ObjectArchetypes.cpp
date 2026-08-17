#include "ObjectArchetypes.h"
#include "../../Math/JsonUtil.h"
#include "../../Externals/imgui/imgui.h" // ImGuizmo.hより先にインクルードする必要がある
#include "../../Externals/ImGuizmo/src/ImGuizmo.h"
#include <algorithm>
#include <cmath>

Vector3 ObjectArchetypes::DirectionToEulerRadians(const Vector3& direction) {
	using namespace DirectX;
	XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&direction));
	XMVECTOR baseDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	float dot = XMVectorGetX(XMVector3Dot(baseDir, dir));
	dot = std::clamp(dot, -1.0f, 1.0f);
	XMVECTOR axis = XMVector3Cross(baseDir, dir);
	XMVECTOR quat;
	if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-8f) {
		// 平行または反対向き：外積が定義できないので特別扱い
		quat = (dot > 0.0f) ? XMQuaternionIdentity()
		                     : XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XM_PI);
	} else {
		quat = XMQuaternionRotationAxis(XMVector3Normalize(axis), acosf(dot));
	}
	Matrix4x4 rotMat;
	XMStoreFloat4x4(&rotMat, XMMatrixRotationQuaternion(quat));

	// 既存のMakeAffineMatrix/Decomposeと同じ経路（行列→DecomposeMatrixToComponents）で変換する
	float t[3], r[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents(&rotMat._11, t, r, s);
	return {
		XMConvertToRadians(r[0]),
		XMConvertToRadians(r[1]),
		XMConvertToRadians(r[2]) };
}

nlohmann::json ObjectArchetypes::MakeCubeArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });

	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json render;
	render["type"] = "CubeRender";
	render["data"] = nlohmann::json::object();
	comps.push_back(render);

	nlohmann::json texSelector;
	texSelector["type"] = "TextureSelector";
	texSelector["data"]["textureName"] = "t.png";
	comps.push_back(texSelector);

	nlohmann::json collider;
	collider["type"] = "OBBCollider";
	collider["data"]["halfSize"] = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	comps.push_back(collider);

	j["components"] = comps;
	return j;
}

nlohmann::json ObjectArchetypes::MakeSphereArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });

	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json render;
	render["type"] = "SphereRender";
	render["data"] = nlohmann::json::object();
	comps.push_back(render);

	nlohmann::json texSelector;
	texSelector["type"] = "TextureSelector";
	texSelector["data"]["textureName"] = "t.png";
	comps.push_back(texSelector);

	nlohmann::json collider;
	collider["type"] = "SphereCollider";
	collider["data"]["radius"] = 1.0f;
	comps.push_back(collider);

	j["components"] = comps;
	return j;
}

nlohmann::json ObjectArchetypes::MakeTriangleArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });

	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json render;
	render["type"] = "TriangleRender";
	render["data"] = nlohmann::json::object();
	comps.push_back(render);

	nlohmann::json texSelector;
	texSelector["type"] = "TextureSelector";
	texSelector["data"]["textureName"] = "t.png";
	comps.push_back(texSelector);

	nlohmann::json collider;
	collider["type"] = "SphereCollider";
	collider["data"]["radius"] = 1.0f;
	comps.push_back(collider);

	j["components"] = comps;
	return j;
}

nlohmann::json ObjectArchetypes::MakeDirectionalLightArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 15.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson(DirectionToEulerRadians({ 0.0f, -1.0f, 0.0f }));
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json light;
	light["type"] = "DirectionalLight";
	light["data"] = nlohmann::json::object();
	comps.push_back(light);
	j["components"] = comps;
	return j;
}

nlohmann::json ObjectArchetypes::MakePointLightArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 2.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json light;
	light["type"] = "PointLight";
	light["data"] = nlohmann::json::object();
	comps.push_back(light);
	j["components"] = comps;
	return j;
}

nlohmann::json ObjectArchetypes::MakeSpotLightArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 2.0f, 3.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson(DirectionToEulerRadians({ -1.0f, -1.0f, 0.0f }));
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json light;
	light["type"] = "SpotLight";
	light["data"] = nlohmann::json::object();
	comps.push_back(light);
	j["components"] = comps;
	return j;
}

nlohmann::json ObjectArchetypes::MakeCameraArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.5f, -10.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f }); // 恒等回転=+Z方向を向く（エンジンの前方軸と一致）
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });
	nlohmann::json comps = nlohmann::json::array();
	nlohmann::json cam;
	cam["type"] = "Camera";
	cam["data"] = nlohmann::json::object();
	comps.push_back(cam);
	j["components"] = comps;
	return j;
}

nlohmann::json ObjectArchetypes::MakeEnemyTemplateArchetype() {
	nlohmann::json j;
	j["transform"]["translation"] = Vector3ToJson({ 0.0f, 1.0f, 0.0f });
	j["transform"]["rotation"]    = Vector3ToJson({ 0.0f, 0.0f, 0.0f });
	j["transform"]["scale"]       = Vector3ToJson({ 1.0f, 1.0f, 1.0f });

	nlohmann::json comps = nlohmann::json::array();

	// 見た目
	nlohmann::json render;
	render["type"] = "CubeRender";
	render["data"] = nlohmann::json::object();
	comps.push_back(render);

	// 当たり判定：isTrigger=trueで押し戻さず検知のみ（プレイヤーの攻撃判定と重なって
	// 消滅させたいだけなので、物理的に押し合う必要はない）
	nlohmann::json collider;
	collider["type"] = "OBBCollider";
	collider["data"]["halfSize"]   = Vector3ToJson({ 0.5f, 0.5f, 0.5f });
	collider["data"]["isTrigger"]  = true;
	collider["data"]["layer"]      = "Default";
	comps.push_back(collider);

	// HP・ヒットシェイク等の敵固有パラメータ＋isTemplate=trueで「テンプレート」の目印にする
	// （Play開始時にPlayScene::OnInitializeがisTemplate=trueのオブジェクトを自動的に隠す）
	nlohmann::json enemy;
	enemy["type"] = "ReflexEnemy";
	enemy["data"]["isTemplate"] = true;
	comps.push_back(enemy);

	j["components"] = comps;
	return j;
}

const std::vector<std::string>& ObjectArchetypes::GetNames() {
	// ここで生成したGameObjectはJSONに残るのは各コンポーネントのtypeName（英語のまま）だけで、
	// このArchetype名自体は保存されないため、GetJson側の分岐と揃えれば自由に日本語化してよい
	static const std::vector<std::string> names = {
		"キューブ", "球", "三角形", "平行光源", "点光源", "スポットライト", "カメラ", "REFLEX敵テンプレート",
	};
	return names;
}

nlohmann::json ObjectArchetypes::GetJson(const std::string& name) {
	if (name == "キューブ")   return MakeCubeArchetype();
	if (name == "球")        return MakeSphereArchetype();
	if (name == "三角形")     return MakeTriangleArchetype();
	if (name == "平行光源")   return MakeDirectionalLightArchetype();
	if (name == "点光源")     return MakePointLightArchetype();
	if (name == "スポットライト") return MakeSpotLightArchetype();
	if (name == "カメラ")     return MakeCameraArchetype();
	if (name == "REFLEX敵テンプレート") return MakeEnemyTemplateArchetype();
	return nlohmann::json::object();
}
