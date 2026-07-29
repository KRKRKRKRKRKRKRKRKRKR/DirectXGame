// 自前OBJ/MTLパーサー。Assimpを介さず学校資料に準拠した最小実装で読み込む
#include "Model.h"
#include <fstream>
#include <sstream>
#include <cassert>

std::unordered_map<std::string, Model::MaterialData> Model::LoadMaterialTemplateFile(
	const std::string& directoryPath, const std::string& filename) {

	// マルチマテリアル対応：newmtlごとに1エントリを登録し、以降のmap_Kdをそのエントリへ書き込む
	std::unordered_map<std::string, MaterialData> materials;
	std::ifstream file(directoryPath + "/" + filename);
	if (!file.is_open()) return materials;

	std::string line;
	std::string currentName;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string identifier;
		s >> identifier;

		if (identifier == "newmtl") {
			s >> currentName;
			materials[currentName] = MaterialData{};

		} else if (identifier == "map_Kd" && !currentName.empty()) {
			std::string textureFilename;
			s >> textureFilename;
			// OBJと同じフォルダにテクスチャがあると想定してパスを結合
			materials[currentName].textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materials;
}

Model::ModelData Model::LoadObjFile(
	const std::string& directoryPath, const std::string& filename) {

	std::vector<Vector3> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	ModelData modelData;

	std::unordered_map<std::string, MaterialData> materials;
	// modelData.subMeshesと1対1対応する、構築中だけ使うマテリアル名の並び
	// （usemtlが一度も無いファイルは空文字列キーのグループ1つになる＝マテリアル無し扱い）
	std::vector<std::string> subMeshMaterialNames;
	std::string currentMaterialName;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string identifier;
		s >> identifier;

		if (identifier == "mtllib") {
			std::string mtlFilename;
			s >> mtlFilename;
			materials = LoadMaterialTemplateFile(directoryPath, mtlFilename);

		} else if (identifier == "usemtl") {
			s >> currentMaterialName;

		} else if (identifier == "v") {
			Vector3 p;
			s >> p.x >> p.y >> p.z;
			p.x *= -1.0f;
			positions.push_back(p);

		} else if (identifier == "vt") {
			Vector2 uv;
			s >> uv.x >> uv.y;
			texcoords.push_back(uv);

		} else if (identifier == "vn") {
			Vector3 n;
			s >> n.x >> n.y >> n.z;
			n.x *= -1.0f;
			normals.push_back(n);

		} else if (identifier == "f") {
			// 直前のサブメッシュと使うマテリアルが違う（または初回の面）なら新しいサブメッシュを
			// 開始する。Blender等のエクスポートは同じマテリアルの面がusemtlごとに連続して
			// 並ぶため、これだけで実用上十分にグループ化できる
			if (subMeshMaterialNames.empty() || subMeshMaterialNames.back() != currentMaterialName) {
				SubMeshData sm;
				sm.vertexOffset = static_cast<uint32_t>(modelData.vertices.size());
				modelData.subMeshes.push_back(sm);
				subMeshMaterialNames.push_back(currentMaterialName);
			}

			// 全頂点トークンを読む（三角形・四角形・n角形対応）
			std::vector<VertexData> faceVerts;
			std::string token;
			while (s >> token) {
				std::istringstream v(token);
				std::string posStr, uvStr, normalStr;
				std::getline(v, posStr,    '/');
				std::getline(v, uvStr,     '/');
				std::getline(v, normalStr, '/');

				VertexData vert{};
				if (!posStr.empty()) {
					Vector3 p = positions[std::stoi(posStr) - 1];
					vert.position = { p.x, p.y, p.z, 1.0f };
				}
				// UVなし（"pos//normal"形式）のときは (0,0) のまま
				if (!uvStr.empty() && !texcoords.empty()) {
					vert.texcoord = texcoords[std::stoi(uvStr) - 1];
					vert.texcoord.y = 1.0f - vert.texcoord.y;
				}
				if (!normalStr.empty()) {
					vert.normal = normals[std::stoi(normalStr) - 1];
				}
				faceVerts.push_back(vert);
			}

			// ファンアルゴリズムで三角形に分割（左手系に合わせて逆順登録）
			for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
				modelData.vertices.push_back(faceVerts[0]);
				modelData.vertices.push_back(faceVerts[i + 1]);
				modelData.vertices.push_back(faceVerts[i]);
				modelData.subMeshes.back().vertexCount += 3;
			}
		}
	}

	// マテリアル名→実データ（テクスチャパス）を解決する
	for (size_t i = 0; i < modelData.subMeshes.size(); ++i) {
		auto it = materials.find(subMeshMaterialNames[i]);
		if (it != materials.end()) modelData.subMeshes[i].material = it->second;
	}

	// "f"行が1個も無いファイルへの安全策：空のサブメッシュを1個用意しておく
	// （Model::Initializeは常に1個以上のsubMeshesを期待するため）
	if (modelData.subMeshes.empty()) {
		modelData.subMeshes.push_back(SubMeshData{});
	}

	return modelData;
}
