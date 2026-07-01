#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "../DescriptorHeaps/DescriptorHeaps.h"

using Microsoft::WRL::ComPtr;

class CommandManager;

class SwapChainManager {
public:
	void Initialize(IDXGIFactory7* factory, ID3D12Device* device, ID3D12CommandQueue* commandQueue, HWND hwnd, int width, int height);

	// ウィンドウリサイズ時に呼ぶ。呼び出し前にGPUの処理完了を待機しておくこと
	void Resize(ID3D12Device* device, int width, int height);

	void BeginTransitionBarrier(ID3D12GraphicsCommandList* commandList);
	void SetRenderTarget(ID3D12GraphicsCommandList* commandList);
	void SetViewport(ID3D12GraphicsCommandList* commandList);
	void EndTransitionBarrier(ID3D12GraphicsCommandList* commandList);
	void Present();

	UINT GetCurrentBackBufferIndex() const { return backBufferIndex_; }
	ID3D12Resource* GetCurrentBackBuffer() const { return swapChainResources_[backBufferIndex_].Get(); }
	DescriptorHeaps* GetDescriptorHeaps() const { return const_cast<DescriptorHeaps*>(&descriptorHeaps_); }

	void Finalize();

private:
	ComPtr<IDXGISwapChain4> swapChain_;
	ComPtr<ID3D12Resource> swapChainResources_[2];
	UINT backBufferIndex_;
	D3D12_RESOURCE_BARRIER barrier_;
	D3D12_VIEWPORT viewport_;
	D3D12_RECT scissorRect_;
	DescriptorHeaps descriptorHeaps_;

	void CreateSwapChain(IDXGIFactory7* factory, ID3D12CommandQueue* commandQueue, HWND hwnd, int width, int height);
	void GetSwapChainResources();
	void InitializeRenderTarget(ID3D12Device* device);
	void SetViewportAndScissorRect(int width, int height);
};
