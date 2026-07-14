#pragma once
#include <Windows.h>
#include <d3d12.h>
#include "../IDrawable.h"
#include "../../Texture/TextureManager.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Pipeline/BlendMode.h"
#include "../../ResourceFactory/InstancedWvpColorBuffer.h"
#include "../../../../Math/MathTypes.h"

class Pipeline;

class Sprite : public IDrawable {
public:
    // maxInstanceCountはSprite2D/Sprite3Dそれぞれ独立に確保する枚数。同一フレーム内で
    // 複数枚描画する場合、index別スロットに書き込まないと全描画が最後の1枚と同じ値を
    // 参照してしまう（Cube/Sphere/Triangleと同じInstancedWvpColorBufferパターンで解決）
    static constexpr uint32_t kMaxInstanceCount = 512;

    void Initialize(ID3D12Device* device, TextureManager* textureManager,
        ID3D12RootSignature* rootSignature, Pipeline* pipeline,
        DescriptorHeaps* heaps, uint32_t wvpHeapIndex, uint32_t colorHeapIndex);

    void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index);
    void SetColor(const Vector4& color, uint32_t index);
    void SetUVTransform(const UVTransform& uvTransform);
    void SetFlipV(bool flip);
    void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
        TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
        bool enableAlphaTest = false, float alphaThreshold = 0.5f);

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

    InstancedWvpColorBuffer wvpColorBuffer_;

    ID3D12RootSignature* rootSignature_ = nullptr;
    Pipeline* pipeline_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    static constexpr int kVertexCount = 4;
    static constexpr int kIndexCount  = 6;

    void CreateVertexResource(ID3D12Device* device);
    void CreateIndexResource(ID3D12Device* device);
    void WriteVertexData();
};
