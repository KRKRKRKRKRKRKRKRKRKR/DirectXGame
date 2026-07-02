// 自前OBJ/MTLパーサー。Assimpを介さず学校資料に準拠した最小実装で読み込む
#include "Model.h"
#include <fstream>
#include <sstream>
#include <cassert>

Model::MaterialData Model::LoadMaterialTemplateFile(
	const std::string& directoryPath, const std::string& filename) {

	MaterialData materialData;
	std::ifstream file(directoryPath + "/" + filename);
	if (!file.is_open()) return materialData;

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string identifier;
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// OBJと同じフォルダにテクスチャがあると想定してパスを結合
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

Model::ModelData Model::LoadObjFile(
	const std::string& directoryPath, const std::string& filename) {

	std::vector<Vector3> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	ModelData modelData;

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
			modelData.material = LoadMaterialTemplateFile(directoryPath, mtlFilename);

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
			}
		}
	}
	return modelData;
}
