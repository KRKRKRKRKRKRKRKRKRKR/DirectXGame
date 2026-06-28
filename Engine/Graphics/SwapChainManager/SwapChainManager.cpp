#include "SwapChainManager.h"
#include "../../Utils/Logger.h"
#include "../../../Math/MathTypes.h"
#include <cassert>

void SwapChainManager::Initialize(IDXGIFactory7* factory, ID3D12Device* device, ID3D12CommandQueue* commandQueue, HWND hwnd, int width, int height) {
	CreateSwapChain(factory, commandQueue, hwnd, width, height);
	GetSwapChainResources();
	descriptorHeaps_.Initialize(device);
	InitializeRenderTarget(device);
	SetViewportAndScissorRect(width, height);
}

void SwapChainManager::CreateSwapChain(IDXGIFactory7* factory, ID3D12CommandQueue* commandQueue, HWND hwnd, int width, int height) {
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ComPtr<IDXGISwapChain1> swapChain;
	HRESULT hr = factory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain);
	if (FAILED(hr)) {
		Logger::Log("Failed CreateSwapChainForHwnd\n");
	}
	assert(SUCCEEDED(hr));

	hr = swapChain->QueryInterface(IID_PPV_ARGS(&swapChain_));
	assert(SUCCEEDED(hr));
}

void SwapChainManager::GetSwapChainResources() {
	backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
	for (int i = 0; i < 2; i++) {
		HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
		if (FAILED(hr)) {
			Logger::Log("Failed GetBuffer\n");
		}
		assert(SUCCEEDED(hr));
	}
}

void SwapChainManager::InitializeRenderTarget(ID3D12Device* device) {
	descriptorHeaps_.CreateRTV(device, swapChainResources_[0].Get(), swapChainResources_[1].Get());
}

void SwapChainManager::SetViewportAndScissorRect(int width, int height) {
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

	scissorRect_.left = 0;
	scissorRect_.top = 0;
	scissorRect_.right = width;
	scissorRect_.bottom = height;
}

void SwapChainManager::BeginTransitionBarrier(ID3D12GraphicsCommandList* commandList) {
	backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
	barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier_.Transition.pResource = swapChainResources_[backBufferIndex_].Get();
	barrier_.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier_);
}

void SwapChainManager::SetRenderTarget(ID3D12GraphicsCommandList* commandList) {
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = descriptorHeaps_.GetRTVHandle(backBufferIndex_);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = descriptorHeaps_.GetDSVHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
	Vector4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	commandList->ClearRenderTargetView(rtvHandle, reinterpret_cast<float*>(&clearColor), 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void SwapChainManager::SetViewport(ID3D12GraphicsCommandList* commandList) {
	commandList->RSSetViewports(1, &viewport_);
	commandList->RSSetScissorRects(1, &scissorRect_);
}

void SwapChainManager::EndTransitionBarrier(ID3D12GraphicsCommandList* commandList) {
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier_);
}

void SwapChainManager::Present() {
	swapChain_->Present(1, 0);
}

void SwapChainManager::Finalize() {
	descriptorHeaps_.Finalize();
	swapChainResources_[0].Reset();
	swapChainResources_[1].Reset();
	swapChain_.Reset();
}
