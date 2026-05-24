#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "ShaderCompiler.h"
#include "Pipline.h"
#include "DescriptorHeaps.h"
using Microsoft::WRL::ComPtr;
class DirectX {
public:
	DirectX() = default;
	~DirectX();
	/// DirectXの初期化
	void Initialize(HWND hwnd, int32_t width, int32_t height);

private:
	//============================
	//===== Factory & Device =====
	//============================
	// Factoryの作成
	void CreateDxgiFactory();
	// アダプタの選択
	void SelectAdapter();
	// デバイスの作成
	void CreateDevice();

	//========================================
	//===== Command Queue & Command List =====
	//========================================
	// コマンドキューとコマンドリストの作成
	void CreateCommandQueueAndList();

	//=====================
	//===== SwapChain =====
	//=====================
	// SwapChainの作成
	void CreateSwapChain(HWND hwnd);
	// SwapChainのリソース取得
	void GetSwapChainResources();
	








	uint32_t windowWidth_ = 0;
	uint32_t windowHeight_ = 0;

	//==========================
	// ===== DXGI & Device =====
	//==========================
	ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;
	ComPtr<ID3D12Device> device_ = nullptr;

	//========================================
	//===== Command Queue & Command List =====
	//========================================
	ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;

	//=====================
	//===== SwapChain =====
	//=====================
	ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	static constexpr UINT backBufferCount_ = 2;
	ComPtr<ID3D12Resource> swapChainResources_[backBufferCount_] = { nullptr, nullptr };

	//==========================
	//===== ShaderCompiler =====
	//==========================
	ShaderCompiler shaderCompiler_;
	
	//===================
	//===== Pipline =====
	//===================
	Pipline pipline_;

	//==========================
	//===== DescriptorHeap =====
	//==========================
	DescriptorHeaps descriptorHeaps_;
};