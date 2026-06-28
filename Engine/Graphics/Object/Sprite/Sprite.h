#pragma once
#include <Windows.h>
#include <d3d12.h>
#include "../IDrawable.h"
#include "../../Texture/TextureManager.h"
#include "../../../../Math/MathTypes.h"
class Sprite : public IDrawable {
public:
    void Initialize(ID3D12Device* device, TextureManager* textureManager,
        ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState);

    void SetWvpMatrix(const Matrix4x4& wvpMatrix);
    void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
        TextureManager* textureManager, TextureHandle texture);

    // IDrawable 実装
    void Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance = 0) override;
    ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
    ID3D12PipelineState* GetPipelineState() const override { return pipelineState_; }

private:
    ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    ComPtr<ID3D12Resource> transformationMatrixResource_;
    ComPtr<ID3D12Resource>materialResource_;
    Matrix4x4* transformationMatrixData_ = nullptr;
	Vector4* materialData_ = nullptr;

    ID3D12RootSignature* rootSignature_ = nullptr;
    ID3D12PipelineState* pipelineState_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    static constexpr int kVertexCount = 6;

    void CreateVertexResource(ID3D12Device* device);
    void WriteVertexData();
    void CreateTransformationMatrixResource(ID3D12Device* device);
	void CreateMaterialResource(ID3D12Device* device);
};
