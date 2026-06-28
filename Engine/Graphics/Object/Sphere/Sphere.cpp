#include "Sphere.h"
#include "../../../../Math/MatrixMath.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include <cassert>

void Sphere::Initialize(ID3D12Device* device, TextureManager* textureManager,
    ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
    uint32_t subdivision, float radius) {
    textureManager_ = textureManager;
    rootSignature_ = rootSignature;
    pipelineState_ = pipelineState;
    CreateVertexResource(device, subdivision, radius);
    CreateWvpResource(device);
    CreateMaterialResource(device);
}

void Sphere::SetWvpMatrix(const Matrix4x4& wvpMatrix) {
    *wvpData_ = wvpMatrix;
}

void Sphere::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
    TextureManager* textureManager, TextureHandle texture) {
    commandList->SetGraphicsRootSignature(rootSignature_);
    commandList->SetPipelineState(pipelineState_);
    commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress()); // 1 = WVP
    commandList->SetGraphicsRootDescriptorTable(2, textureManager->GetSrvGpuHandle(texture)); // 2 = Texture
}

void Sphere::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance) {
    (void)instanceCount; (void)startInstance;
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress()); // 0 = Material
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(vertexCount_, 1, 0, 0);
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

            // 4頂点 a,b,c,d を計算
            Vector3 a = { radius * cosf(lat) * cosf(lon),            radius * sinf(lat),            radius * cosf(lat) * sinf(lon) };
            Vector3 b = { radius * cosf(lat + kLatEvery) * cosf(lon),            radius * sinf(lat + kLatEvery), radius * cosf(lat + kLatEvery) * sinf(lon) };
            Vector3 c = { radius * cosf(lat) * cosf(lon + kLonEvery), radius * sinf(lat),            radius * cosf(lat) * sinf(lon + kLonEvery) };
            Vector3 d = { radius * cosf(lat + kLatEvery) * cosf(lon + kLonEvery), radius * sinf(lat + kLatEvery), radius * cosf(lat + kLatEvery) * sinf(lon + kLonEvery) };

            // UV
            float u0 = lonIndex / static_cast<float>(subdivision);
            float u1 = (lonIndex + 1) / static_cast<float>(subdivision);
            float v0 = 1.0f - latIndex / static_cast<float>(subdivision);
            float v1 = 1.0f - (latIndex + 1) / static_cast<float>(subdivision);

            // 三角形1: a, b, c
            vertexData[start + 0] = { {a.x, a.y, a.z, 1.0f}, {u0, v0} };
            vertexData[start + 1] = { {b.x, b.y, b.z, 1.0f}, {u0, v1} };
            vertexData[start + 2] = { {c.x, c.y, c.z, 1.0f}, {u1, v0} };
            // 三角形2: c, b, d
            vertexData[start + 3] = { {c.x, c.y, c.z, 1.0f}, {u1, v0} };
            vertexData[start + 4] = { {b.x, b.y, b.z, 1.0f}, {u0, v1} };
            vertexData[start + 5] = { {d.x, d.y, d.z, 1.0f}, {u1, v1} };
        }
    }

    vertexResource_->Unmap(0, nullptr);
}

void Sphere::CreateWvpResource(ID3D12Device* device) {
    wvpResource_ = ResourceFactory::CreateBufferResource(device, sizeof(Matrix4x4));
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    *wvpData_ = MatrixMath::Identity();
}

void Sphere::CreateMaterialResource(ID3D12Device* device) {
    materialResource_ = ResourceFactory::CreateBufferResource(device, sizeof(Vector4));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    *materialData_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白
}
