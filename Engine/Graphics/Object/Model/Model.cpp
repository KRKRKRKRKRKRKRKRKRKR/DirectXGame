#include "Model.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../../Utils/Logger.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstring>

// ---- デストラクタ ----

Model::~Model() {
	if (wvpResource_ && wvpMappedData_) {
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
	}
	if (colorResource_ && colorMappedData_) {
		colorResource_->Unmap(0, nullptr);
		colorMappedData_ = nullptr;
	}
	vertexResource_.Reset();
}

// ---- 初期化 ----

void Model::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
	DescriptorHeaps* heaps,
	const std::string& directoryPath, const std::string& filename,
	uint32_t wvpHeapIndex, uint32_t colorHeapIndex) {

	textureManager_ = textureManager;
	rootSignature_  = rootSignature;
	pipelineState_  = pipelineState;

	ModelData modelData = LoadObjFile(directoryPath, filename);
	vertexCount_ = static_cast<uint32_t>(modelData.vertices.size());

	// MTLで指定されたテクスチャをロード（ファイルが存在する場合のみ）
	if (!modelData.material.textureFilePath.empty()) {
		std::ifstream check(modelData.material.textureFilePath);
		if (check.is_open()) {
			textureHandle_ = textureManager->Load(modelData.material.textureFilePath, heaps);
		}
	}

	CreateVertexResource(device, modelData);
	CreateWvpResource(device);
	CreateColorResource(device);

	auto wvpSrv   = heaps->CreateStructuredBufferSRV(device, wvpResource_.Get(),   kMaxInstanceCount, sizeof(TransformationMatrix), wvpHeapIndex);
	auto colorSrv = heaps->CreateStructuredBufferSRV(device, colorResource_.Get(), kMaxInstanceCount, sizeof(Vector4),   colorHeapIndex);

	wvpSrvHandle_   = wvpSrv.gpuHandle;
	colorSrvHandle_ = colorSrv.gpuHandle;

	Logger::Log("Model loaded: " + directoryPath + "/" + filename +
		" | " + std::to_string(vertexCount_) + " vertices\n");
}

// ---- MTLパース（学校資料の LoadMaterialTemplateFile 相当）----

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

// ---- OBJパース ----

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

// ---- GPUリソース作成 ----

void Model::CreateVertexResource(ID3D12Device* device, const ModelData& modelData) {
	vertexResource_ = ResourceFactory::CreateBufferResource(device, sizeof(VertexData) * vertexCount_);
	assert(vertexResource_);

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes    = static_cast<UINT>(sizeof(VertexData) * vertexCount_);
	vertexBufferView_.StrideInBytes  = sizeof(VertexData);

	VertexData* mapped = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	assert(SUCCEEDED(hr) && mapped);
	std::memcpy(mapped, modelData.vertices.data(), sizeof(VertexData) * vertexCount_);
	vertexResource_->Unmap(0, nullptr);
}

void Model::CreateWvpResource(ID3D12Device* device) {
	wvpStride_   = sizeof(TransformationMatrix);
	wvpResource_ = ResourceFactory::CreateBufferResource(device, wvpStride_ * kMaxInstanceCount);
	assert(wvpResource_);

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	assert(SUCCEEDED(hr) && wvpMappedData_);
}

void Model::CreateColorResource(ID3D12Device* device) {
	colorStride_   = sizeof(Vector4);
	colorResource_ = ResourceFactory::CreateBufferResource(device, colorStride_ * kMaxInstanceCount);
	assert(colorResource_);

	HRESULT hr = colorResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorMappedData_));
	assert(SUCCEEDED(hr) && colorMappedData_);

	// デフォルト白
	for (uint32_t i = 0; i < kMaxInstanceCount; i++) {
		Vector4* dst = reinterpret_cast<Vector4*>(colorMappedData_ + i * colorStride_);
		*dst = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

// ---- 描画 ----

void Model::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	if (!wvpMappedData_) return;
	TransformationMatrix* dst = reinterpret_cast<TransformationMatrix*>(
		reinterpret_cast<char*>(wvpMappedData_) + index * wvpStride_);
	dst->WVP   = wvpMatrix;
	dst->World = world;
}

void Model::SetColor(const Vector4& color, uint32_t index) {
	if (!colorMappedData_) return;
	Vector4* dst = reinterpret_cast<Vector4*>(colorMappedData_ + index * colorStride_);
	*dst = color;
}

void Model::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture) {
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(pipelineState_);
	commandList->SetGraphicsRootDescriptorTable(0, textureManager->GetSrvGpuHandle(texture));
	commandList->SetGraphicsRootDescriptorTable(1, wvpSrvHandle_);
	commandList->SetGraphicsRootDescriptorTable(2, colorSrvHandle_);
}

void Model::Draw(ID3D12GraphicsCommandList* commandList,
	uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(vertexCount_, instanceCount, 0, startInstance);
}
