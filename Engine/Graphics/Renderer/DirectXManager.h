#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <cstdint>
#include <string>
#include <wrl.h>
#include "../../../Externals/DirectXTex/DirectXTex.h"
#include "../../../Externals/DirectXTex/d3dx12.h"
#include "../DirectXDevice/DirectXDevice.h"
#include "../CommandManager/CommandManager.h"
#include "../SwapChainManager/SwapChainManager.h"
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../Pipeline/Pipeline.h"
#include "../Pipeline/LinePipeline.h"
#include "../Texture/TextureManager.h"
#include "../Object/Triangle/Triangle.h"
#include "../Object/Line/Line.h"
#include "../Object/Sprite/Sprite.h"
#include "../Object/Sphere/Sphere.h"
#include "../../../Math/MathTypes.h"
#include "../../../Math/TransformMath.h"

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

class DirectXManager {
public:
	DirectXManager() = default;
	~DirectXManager();

	static constexpr uint32_t kMaxWvpCount = 100;

	void Initialize(HWND hwnd, int width, int height);
	void BeginFrame();
	void EndFrame();
	void Finalize();

	// Drawing methods (same interface as DirectXManager)
	void DrawTriangleRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, const Vector4& color, TextureID textureID);
	void DrawLineRender(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& start, const Vector3& end, const Vector4& color);
	void DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection);
	void DrawSpriteRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
	void DrawSphereRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, TextureID textureID);

	// Getters (same interface as DirectXManager)
	ID3D12Device* GetDevice() const { return device_.GetDevice(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return command_.GetCommandList(); }
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const { return swapChain_.GetDescriptorHeaps()->GetSRVDescriptorHeap(); }
	const D3D12_DESCRIPTOR_RANGE* GetDescriptorRange() const { return descriptorRange_; }
	UINT GetDescriptorRangeCount() const { return DESCRIPTOR_RANGE_COUNT; }
	const D3D12_STATIC_SAMPLER_DESC* GetStaticSamplers() const { return staticSamplers_; }
	UINT GetStaticSamplerCount() const { return STATIC_SAMPLER_COUNT; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return swapChain_.GetDescriptorHeaps()->GetDSVHandle(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return swapChain_.GetDescriptorHeaps()->GetRTVHandle(swapChain_.GetCurrentBackBufferIndex()); }
	UINT GetBackBufferIndex() const { return swapChain_.GetCurrentBackBufferIndex(); }
	Triangle* GetTriangle() { return triangle_.get(); }
	TextureManager* GetTextureManager() { return &textureManager_; }
	int32_t GetClientWidth() const { return windowWidth_; }
	int32_t GetClientHeight() const { return windowHeight_; }

	void InitializeGridLines();

private:
	DirectXDevice device_;
	CommandManager command_;
	SwapChainManager swapChain_;
	ShaderCompiler shaderCompiler_;
	Pipeline pipeline_;
	LinePipeline linePipeline_;
	TextureManager textureManager_;
	std::unique_ptr<Triangle> triangle_;
	std::unique_ptr<Line> line_;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Sphere> sphere_;

	uint32_t currentTriangleWvpIndex_ = 0;
	uint32_t currentLineWvpIndex_ = 0;
	int windowWidth_ = 0;
	int windowHeight_ = 0;
	bool initialized_ = false;
	bool comInitialized_ = false;

	// Pipeline descriptor ranges
	static constexpr UINT DESCRIPTOR_RANGE_COUNT = 1;
	D3D12_DESCRIPTOR_RANGE descriptorRange_[DESCRIPTOR_RANGE_COUNT] = {};
	static constexpr UINT STATIC_SAMPLER_COUNT = 1;
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[STATIC_SAMPLER_COUNT] = {};

	void InitializeTexture();
	void WaitForGPUCompletion();
};
