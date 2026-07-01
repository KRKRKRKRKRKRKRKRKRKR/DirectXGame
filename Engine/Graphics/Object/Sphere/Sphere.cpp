#include "Sphere.h"
#include "../../../../Math/MatrixMath.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../Pipeline/Pipeline.h"
#include <cassert>
#include <cstring>

Sphere::~Sphere() {
    if (wvpResource_ && wvpMappedData_) {
        wvpResource_->Unmap(0, nullptr);
        wvpMappedData_ = nullptr;
        wvpResource_.Reset();
    }
    if (colorResource_ && colorMappedData_) {
        colorResource_->Unmap(0, nullptr);
        colorMappedData_ = nullptr;
        colorResource_.Reset();
    }
    vertexResource_.Reset();
}

void Sphere::Initialize(ID3D12Device* device, TextureManager* textureManager,
    ID3D12RootSignature* rootSignature, Pipeline* pipeline,
    DescriptorHeaps* heaps,
    uint32_t subdivision, float radius) {
    textureManager_ = textureManager;
    rootSignature_ = rootSignature;
    pipeline_ = pipeline;

    CreateVertexResource(device, subdivision, radius);
    CreateWvpResource(device);
    CreateColorResource(device);

    // SRVをDescriptorHeapsに登録（index 14: WVP, index 15: 色）
    auto wvpSrv = heaps->CreateStructuredBufferSRV(
        device, wvpResource_.Get(), kMaxInstanceCount, sizeof(TransformationMatrix), 14);
    auto colorSrv = heaps->CreateStructuredBufferSRV(
        device, colorResource_.Get(), kMaxInstanceCount, sizeof(Vector4), 15);

    wvpSrvHandle_ = wvpSrv.gpuHandle;
    colorSrvHandle_ = colorSrv.gpuHandle;
}

void Sphere::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
    if (!wvpMappedData_) return;
    TransformationMatrix* dst = reinterpret_cast<TransformationMatrix*>(
        wvpMappedData_ + index * wvpStride_);
    dst->WVP   = wvpMatrix;
    dst->World = world;
}

void Sphere::SetColor(const Vector4& color, uint32_t index) {
    if (!colorMappedData_) return;
    Vector4* data = reinterpret_cast<Vector4*>(colorMappedData_ + index * colorStride_);
    *data = color;
}

void Sphere::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
    TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength) {
    commandList->SetGraphicsRootSignature(rootSignature_);
    commandList->SetPipelineState(pipeline_->GetPipelineState(blendMode));
    const float blendFactor[4] = { blendStrength, blendStrength, blendStrength, blendStrength };
    commandList->OMSetBlendFactor(blendFactor);
    commandList->SetGraphicsRootDescriptorTable(0, textureManager->GetSrvGpuHandle(texture)); // t0: テクスチャ
    commandList->SetGraphicsRootDescriptorTable(1, wvpSrvHandle_);                              // t1: WVP
    commandList->SetGraphicsRootDescriptorTable(2, colorSrvHandle_);                            // t2: 色
}

ID3D12PipelineState* Sphere::GetPipelineState() const {
    return pipeline_->GetPipelineState(BlendMode::kNone);
}

void Sphere::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance) {
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(vertexCount_, instanceCount, 0, startInstance);
}

void Sphere::CreateVertexResource(ID3D12Device* device, uint32_t subdivision, float radius) {
    vertexCount_ = subdivision * subdivision * 6;
    const size_t bufferSize = sizeof(VertexData) * vertexCount_;

    vertexResource_ = ResourceFactory::CreateBufferResource(device, bufferSize);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    assert(SUCCEEDED(hr) && vertexData);

    const float kLonEvery = DirectX::XM_2PI / subdivision;
    const float kLatEvery = DirectX::XM_PI / subdivision;

    for (uint32_t latIndex = 0; latIndex < subdivision; ++latIndex) {
        float lat = -DirectX::XM_PIDIV2 + latIndex * kLatEvery;
        for (uint32_t lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
            uint32_t start = (latIndex * subdivision + lonIndex) * 6;
            float lon = lonIndex * kLonEvery;

            Vector3 a = { radius * cosf(lat) * cosf(lon),            radius * sinf(lat),            radius * cosf(lat) * sinf(lon) };
            Vector3 b = { radius * cosf(lat + kLatEvery) * cosf(lon),            radius * sinf(lat + kLatEvery), radius * cosf(lat + kLatEvery) * sinf(lon) };
            Vector3 c = { radius * cosf(lat) * cosf(lon + kLonEvery), radius * sinf(lat),            radius * cosf(lat) * sinf(lon + kLonEvery) };
            Vector3 d = { radius * cosf(lat + kLatEvery) * cosf(lon + kLonEvery), radius * sinf(lat + kLatEvery), radius * cosf(lat + kLatEvery) * sinf(lon + kLonEvery) };

            float u0 = lonIndex / static_cast<float>(subdivision);
            float u1 = (lonIndex + 1) / static_cast<float>(subdivision);
            float v0 = 1.0f - latIndex / static_cast<float>(subdivision);
            float v1 = 1.0f - (latIndex + 1) / static_cast<float>(subdivision);

            // 三角形1: a, b, c
            vertexData[start + 0] = { {a.x, a.y, a.z, 1.0f}, {u0, v0}, {a.x / radius, a.y / radius, a.z / radius} };
            vertexData[start + 1] = { {b.x, b.y, b.z, 1.0f}, {u0, v1}, {b.x / radius, b.y / radius, b.z / radius} };
            vertexData[start + 2] = { {c.x, c.y, c.z, 1.0f}, {u1, v0}, {c.x / radius, c.y / radius, c.z / radius} };

            // 三角形2: c, b, d
            vertexData[start + 3] = { {c.x, c.y, c.z, 1.0f}, {u1, v0}, {c.x / radius, c.y / radius, c.z / radius} };
            vertexData[start + 4] = { {b.x, b.y, b.z, 1.0f}, {u0, v1}, {b.x / radius, b.y / radius, b.z / radius} };
            vertexData[start + 5] = { {d.x, d.y, d.z, 1.0f}, {u1, v1}, {d.x / radius, d.y / radius, d.z / radius} };
        }
    }

    vertexResource_->Unmap(0, nullptr);
}

void Sphere::CreateWvpResource(ID3D12Device* device) {
    wvpStride_ = sizeof(TransformationMatrix);
    wvpResource_ = ResourceFactory::CreateBufferResource(device, wvpStride_ * kMaxInstanceCount);
    assert(wvpResource_);

    HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
    assert(SUCCEEDED(hr) && wvpMappedData_);
}

void Sphere::CreateColorResource(ID3D12Device* device) {
    colorStride_ = sizeof(Vector4);
    colorResource_ = ResourceFactory::CreateBufferResource(device, colorStride_ * kMaxInstanceCount);
    assert(colorResource_);

    HRESULT hr = colorResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorMappedData_));
    assert(SUCCEEDED(hr) && colorMappedData_);

    // デフォルト白
    for (uint32_t i = 0; i < kMaxInstanceCount; i++) {
        Vector4* data = reinterpret_cast<Vector4*>(colorMappedData_ + i * colorStride_);
        *data = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}
