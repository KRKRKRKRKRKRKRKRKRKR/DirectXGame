#pragma once
#include <Windows.h>
#include <d3d12.h>
#include "../IDrawable.h"
#include "../../Texture/TextureManager.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Pipeline/BlendMode.h"
#include "../../../../Math/MathTypes.h"

class Pipeline;

class Sprite : public IDrawable {
public:
    void Initialize(ID3D12Device* device, TextureManager* textureManager,
        ID3D12RootSignature* rootSignature, Pipeline* pipeline,
        DescriptorHeaps* heaps, uint32_t wvpHeapIndex, uint32_t colorHeapIndex);

    void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world);
    void SetColor(const Vector4& color);
    void SetUVTransform(const UVTransform& uvTransform);
    void SetFlipV(bool flip);
    void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
        TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f);

    // IDrawable 実装
    void Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance = 0) override;
    ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
    ID3D12PipelineState* GetPipelineState() const override;

private:
    ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    VertexData* vertexMappedData_ = nullptr;
    bool flipV_ = false;

    ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    ComPtr<ID3D12Resource> wvpResource_;
    TransformationMatrix* wvpMappedData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE wvpSrvHandle_{};

    ComPtr<ID3D12Resource> colorResource_;
    Vector4* colorMappedData_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle_{};

    ID3D12RootSignature* rootSignature_ = nullptr;
    Pipeline* pipeline_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    static constexpr int kVertexCount = 4;
    static constexpr int kIndexCount  = 6;

    void CreateVertexResource(ID3D12Device* device);
    void CreateIndexResource(ID3D12Device* device);
    void WriteVertexData();
    void CreateWvpResource(ID3D12Device* device);
    void CreateColorResource(ID3D12Device* device);
};
