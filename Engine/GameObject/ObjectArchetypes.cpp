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

const std::vector<std::string>& ObjectArchetypes::GetNames() {
	static const std::vector<std::string> names = {
		"Cube", "Sphere", "Triangle", "Directional Light", "Point Light", "Spot Light",
	};
	return names;
}

nlohmann::json ObjectArchetypes::GetJson(const std::string& name) {
	if (name == "Cube")              return MakeCubeArchetype();
	if (name == "Sphere")            return MakeSphereArchetype();
	if (name == "Triangle")          return MakeTriangleArchetype();
	if (name == "Directional Light") return MakeDirectionalLightArchetype();
	if (name == "Point Light")       return MakePointLightArchetype();
	if (name == "Spot Light")        return MakeSpotLightArchetype();
	return nlohmann::json::object();
}
