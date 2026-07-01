#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"
#include "../../Texture/TextureManager.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Pipeline/BlendMode.h"

using Microsoft::WRL::ComPtr;

class Pipeline;

class Model : public IDrawable {
public:
	Model() = default;
	virtual ~Model();

	static constexpr uint32_t kMaxInstanceCount = 4096;

	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, Pipeline* pipeline,
		DescriptorHeaps* heaps,
		const std::string& directoryPath, const std::string& filename,
		uint32_t wvpHeapIndex, uint32_t colorHeapIndex);

	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index = 0);
	void SetColor(const Vector4& color, uint32_t index = 0);
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
		TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f);

	TextureHandle GetTextureHandle() const { return textureHandle_; }

	void Draw(ID3D12GraphicsCommandList* commandList,
		uint32_t instanceCount, uint32_t startInstance = 0) override;

	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override;

private:
	// 学校資料に準拠した構造体
	struct MaterialData {
		std::string textureFilePath;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData            material;
	};

	uint32_t      vertexCount_   = 0;
	TextureHandle textureHandle_ = kTextureNone;

	ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	ComPtr<ID3D12Resource> wvpResource_;
	uint8_t*  wvpMappedData_ = nullptr;
	uint32_t  wvpStride_     = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE wvpSrvHandle_{};

	ComPtr<ID3D12Resource> colorResource_;
	uint8_t*  colorMappedData_ = nullptr;
	uint32_t  colorStride_     = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle_{};

	ID3D12RootSignature* rootSignature_  = nullptr;
	Pipeline*            pipeline_       = nullptr;
	TextureManager*      textureManager_ = nullptr;

	// MTLファイルからテクスチャパスを読む（学校資料の LoadMaterialTemplateFile 相当）
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
		const std::string& filename);

	// OBJファイルを読む
	ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	void CreateVertexResource (ID3D12Device* device, const ModelData& modelData);
	void CreateWvpResource    (ID3D12Device* device);
	void CreateColorResource  (ID3D12Device* device);
};
