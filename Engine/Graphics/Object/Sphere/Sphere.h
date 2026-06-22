#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../Texture/TextureManager.h"
#include "../../../../Math/MathTypes.h"

using Microsoft::WRL::ComPtr;

class Sphere : public IDrawable {
public:
    void Initialize(ID3D12Device* device, TextureManager* textureManager,
        ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
        uint32_t subdivision = 30, float radius = 1.0f);

    void SetWvpMatrix(const Matrix4x4& wvpMatrix);
    void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
        TextureManager* textureManager, TextureID textureID);

    // IDrawable 実装
    void Draw(ID3D12GraphicsCommandList* commandList, uint32_t wvpIndex) override;
    ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
    ID3D12PipelineState* GetPipelineState() const override { return pipelineState_; }

private:
    ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    ComPtr<ID3D12Resource> wvpResource_;
    Matrix4x4* wvpData_ = nullptr;
    ComPtr<ID3D12Resource> materialResource_;
    Vector4* materialData_ = nullptr;
    uint32_t vertexCount_ = 0;

    ID3D12RootSignature* rootSignature_ = nullptr;
    ID3D12PipelineState* pipelineState_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    void CreateVertexResource(ID3D12Device* device, uint32_t subdivision, float radius);
    void CreateWvpResource(ID3D12Device* device);
    void CreateMaterialResource(ID3D12Device* device);
};
