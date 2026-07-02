#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../Texture/TextureManager.h"
#include "../../../../Math/MathTypes.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Pipeline/BlendMode.h"
using Microsoft::WRL::ComPtr;

class TextureManager;
class Pipeline;

class Sphere : public IDrawable {
public:
    Sphere() = default;
    virtual ~Sphere();

    static constexpr uint32_t kMaxInstanceCount = 4096;

    void Initialize(ID3D12Device* device, TextureManager* textureManager,
        ID3D12RootSignature* rootSignature, Pipeline* pipeline,
        DescriptorHeaps* heaps,
        uint32_t subdivision = 30, float radius = 1.0f);

    void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index);
    void SetColor(const Vector4& color, uint32_t index);
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

    ComPtr<ID3D12Resource> wvpResource_;
    uint8_t* wvpMappedData_ = nullptr;
    uint32_t wvpStride_ = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE wvpSrvHandle_{};

    ComPtr<ID3D12Resource> colorResource_;
    uint8_t* colorMappedData_ = nullptr;
    uint32_t colorStride_ = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle_{};

    uint32_t vertexCount_ = 0;

    ID3D12RootSignature* rootSignature_ = nullptr;
    Pipeline* pipeline_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    void CreateVertexResource(ID3D12Device* device, uint32_t subdivision, float radius);
    void CreateWvpResource(ID3D12Device* device);
    void CreateColorResource(ID3D12Device* device);
};
