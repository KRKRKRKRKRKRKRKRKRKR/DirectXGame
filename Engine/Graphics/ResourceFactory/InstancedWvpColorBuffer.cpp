#include "InstancedWvpColorBuffer.h"
#include "ResourceFactory.h"
#include "../../../Math/MatrixMath.h"
#include <cassert>
#include <cstring>

InstancedWvpColorBuffer::~InstancedWvpColorBuffer() {
	if (wvpResource_ && wvpMappedData_) {
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
	}
	if (colorResource_ && colorMappedData_) {
		colorResource_->Unmap(0, nullptr);
		colorMappedData_ = nullptr;
	}
}

void InstancedWvpColorBuffer::Initialize(ID3D12Device* device, DescriptorHeaps* heaps,
	uint32_t maxInstanceCount, uint32_t wvpHeapIndex, uint32_t colorHeapIndex) {

	CreateWvpResource(device, maxInstanceCount);
	CreateColorResource(device, maxInstanceCount);

	auto wvpSrv   = heaps->CreateStructuredBufferSRV(device, wvpResource_.Get(),   maxInstanceCount, sizeof(TransformationMatrix), wvpHeapIndex);
	auto colorSrv = heaps->CreateStructuredBufferSRV(device, colorResource_.Get(), maxInstanceCount, sizeof(Vector4),              colorHeapIndex);

	wvpSrvHandle_   = wvpSrv.gpuHandle;
	colorSrvHandle_ = colorSrv.gpuHandle;
}

void InstancedWvpColorBuffer::CreateWvpResource(ID3D12Device* device, uint32_t maxInstanceCount) {
	wvpStride_   = sizeof(TransformationMatrix);
	wvpResource_ = ResourceFactory::CreateBufferResource(device, static_cast<size_t>(wvpStride_) * maxInstanceCount);
	assert(wvpResource_);

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	assert(SUCCEEDED(hr) && wvpMappedData_);
}

void InstancedWvpColorBuffer::CreateColorResource(ID3D12Device* device, uint32_t maxInstanceCount) {
	colorStride_   = sizeof(Vector4);
	colorResource_ = ResourceFactory::CreateBufferResource(device, static_cast<size_t>(colorStride_) * maxInstanceCount);
	assert(colorResource_);

	HRESULT hr = colorResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorMappedData_));
	assert(SUCCEEDED(hr) && colorMappedData_);

	// デフォルト白
	for (uint32_t i = 0; i < maxInstanceCount; i++) {
		Vector4* dst = reinterpret_cast<Vector4*>(colorMappedData_ + i * colorStride_);
		*dst = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void InstancedWvpColorBuffer::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	SetWvpMatrix(wvpMatrix, world, MatrixMath::Transpose(MatrixMath::Inverse(world)), index);
}

void InstancedWvpColorBuffer::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index) {
	if (!wvpMappedData_) return;
	TransformationMatrix* dst = reinterpret_cast<TransformationMatrix*>(
		wvpMappedData_ + index * wvpStride_);
	dst->WVP   = wvpMatrix;
	dst->World = world;
	dst->WorldInverseTranspose = worldInverseTranspose;
}

void InstancedWvpColorBuffer::SetColor(const Vector4& color, uint32_t index) {
	if (!colorMappedData_) return;
	Vector4* dst = reinterpret_cast<Vector4*>(colorMappedData_ + index * colorStride_);
	*dst = color;
}
