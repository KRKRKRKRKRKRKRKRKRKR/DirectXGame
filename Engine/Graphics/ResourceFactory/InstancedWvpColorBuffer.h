#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../../../Math/MathTypes.h"
#include "../DescriptorHeaps/DescriptorHeaps.h"

using Microsoft::WRL::ComPtr;

// Cube/Sphere/Triangle/Model等が共通して持つ「インスタンスごとのWVP行列 + 色」の
// StructuredBufferペアをまとめたヘルパー。継承ではなくコンポジションで持たせる
// （各Objectクラスは互いに無関係のIDrawable実装であり、共通の親を作ると関係性が歪むため）。
class InstancedWvpColorBuffer {
public:
	~InstancedWvpColorBuffer();

	// maxInstanceCountはCube/Sphere/Triangle/Modelで共通の4096。
	// wvpHeapIndex/colorHeapIndexはDescriptorHeaps上のSRV登録先index
	void Initialize(ID3D12Device* device, DescriptorHeaps* heaps,
		uint32_t maxInstanceCount, uint32_t wvpHeapIndex, uint32_t colorHeapIndex);

	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index);

	// worldInverseTransposeを呼び出し側で計算済みの場合はこちらを使う（Inverse+Transpose計算を省略できる）。
	// 大量の静止インスタンス（グリッド等）で回転が変化した時だけ再計算してキャッシュする用途向け
	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index);

	void SetColor(const Vector4& color, uint32_t index);

	D3D12_GPU_DESCRIPTOR_HANDLE GetWvpSrvHandle() const { return wvpSrvHandle_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetColorSrvHandle() const { return colorSrvHandle_; }

private:
	void CreateWvpResource(ID3D12Device* device, uint32_t maxInstanceCount);
	void CreateColorResource(ID3D12Device* device, uint32_t maxInstanceCount);

	ComPtr<ID3D12Resource> wvpResource_;
	uint8_t*  wvpMappedData_ = nullptr;
	uint32_t  wvpStride_     = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE wvpSrvHandle_{};

	ComPtr<ID3D12Resource> colorResource_;
	uint8_t*  colorMappedData_ = nullptr;
	uint32_t  colorStride_     = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle_{};
};
