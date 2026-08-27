#include "DirectXManager.h"
#include "../../Utils/Logger.h"
#include <cassert>

DirectXManager::~DirectXManager() {
	Finalize();
}

void DirectXManager::Initialize(HWND hwnd, int width, int height) {
	windowWidth_ = width;
	windowHeight_ = height;
	if (initialized_) {
		Logger::Log("Already initialized\n");
		return;
	}

	device_.Initialize();
	command_.Initialize(device_.GetDevice());
	swapChain_.Initialize(device_.GetDxgiFactory(), device_.GetDevice(), command_.GetCommandQueue(), hwnd, width, height);

	Logger::Log("Complete Initialize DirectXManager\n");
	initialized_ = true;
}

void DirectXManager::BeginFrame() {
	ID3D12GraphicsCommandList* commandList = command_.GetCommandList();

	swapChain_.BeginTransitionBarrier(commandList);
	swapChain_.SetRenderTarget(commandList);
	swapChain_.SetViewport(commandList);

	ID3D12DescriptorHeap* descriptorHeaps[] = { swapChain_.GetDescriptorHeaps()->GetSRVDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);
}

void DirectXManager::EndFrame() {
	ID3D12GraphicsCommandList* commandList = command_.GetCommandList();

	swapChain_.EndTransitionBarrier(commandList);

	HRESULT hr = commandList->Close();
	if (FAILED(hr)) {
		Logger::Log("Failed Close CommandList\n");
	}
	assert(SUCCEEDED(hr));

	command_.ExecuteCommandList();
	command_.Signal();
	swapChain_.Present();

	command_.WaitForFence();
	command_.ResetCommandList();
}

void DirectXManager::Resize(int width, int height) {
	if (!initialized_ || width <= 0 || height <= 0) {
		return;
	}
	windowWidth_ = width;
	windowHeight_ = height;

	// コマンドリストは通常このタイミングでオープン中(次フレーム用にリセット済み)なので、
	// GPU待機の前に一旦閉じて、積まれている分（ロード中のテクスチャ転送やリソースバリア等）を
	// 実行してからでないとGPU待機の意味が無い（ExecuteCommandListを呼ばずSignalだけしても
	// 何もキューに送られておらず、ResetCommandList内のAllocator::Resetで未実行コマンドごと
	// 破棄されてしまう。これがロード中の最大化操作でGPU-Based Validationの
	// Incompatible texture barrier layoutエラーを引き起こしていた）
	ID3D12GraphicsCommandList* commandList = command_.GetCommandList();
	commandList->Close();

	command_.ExecuteCommandList();
	command_.Signal();
	command_.WaitForFence();

	swapChain_.Resize(device_.GetDevice(), width, height);

	command_.ResetCommandList();
}

void DirectXManager::Finalize() {
	if (!initialized_) {
		return;
	}
	initialized_ = false;

	command_.Signal();
	command_.WaitForFence();

	swapChain_.Finalize();
	device_.Finalize();

	Logger::Log("Complete Finalize DirectXManager\n");
}

void DirectXManager::WaitForGPUCompletion() {
	ID3D12GraphicsCommandList* commandList = command_.GetCommandList();

	HRESULT hr = commandList->Close();
	if (FAILED(hr) && hr != E_FAIL) {
		Logger::Log("Failed Close CommandList\n");
		return;
	}

	command_.ExecuteCommandList();
	command_.Signal();
	command_.WaitForFence();
	command_.ResetCommandList();
}
