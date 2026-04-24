#include "DirectXManager.h"
#include "../Utils/Logger.h"
#include "../Utils/StringUtils.h"
#include <cassert>
#include <format>

DirectXManager::~DirectXManager() {

	CloseHandle(fenceEvent_);
	if(fence_) { fence_->Release(); fence_ = nullptr; }
	if (rtvDescriptorHeap_) { rtvDescriptorHeap_->Release(); rtvDescriptorHeap_ = nullptr; }
	if (swapChainResources_[0]) { swapChainResources_[0]->Release();  swapChainResources_[0] = nullptr; }
	if (swapChainResources_[1]) { swapChainResources_[1]->Release();  swapChainResources_[1] = nullptr; }
	if (swapChain_) { swapChain_->Release();	swapChain_ = nullptr; }
	if (commandList_) { commandList_->Release();	commandList_ = nullptr; }
	if (commandAllocator_) { commandAllocator_->Release();	commandAllocator_ = nullptr; }
	if (commandQueue_) { commandQueue_->Release();	commandQueue_ = nullptr; }
	if (device_) { device_->Release();	device_ = nullptr; }
	if (useAdapter_) { useAdapter_->Release();	useAdapter_ = nullptr; }
	if (dxgiFactory_) { dxgiFactory_->Release();	dxgiFactory_ = nullptr; }
}

void DirectXManager::Initialize(HWND hwnd, int32_t width, int32_t height) {
	CreateFactory();
	SelectAdapter();
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain(hwnd, width, height);
	GetSwapChainResources();
	CreateDescriptorHeap();
	CreateRTV();
	CreateFence();
	Logger::Log("Complete Initialize DirectXManager\n");
}

void DirectXManager::CreateFactory() {
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDXGIFactory\n");
	}

	assert(SUCCEEDED(hr));
}

void DirectXManager::SelectAdapter() {
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) != DXGI_ERROR_NOT_FOUND; ++i) {

		DXGI_ADAPTER_DESC3 adapterDesc{};
		HRESULT hr = useAdapter_->GetDesc3(&adapterDesc);
		if (FAILED(hr)) {
			Logger::Log("Failed GetDesc3\n");
		}
		assert(SUCCEEDED(hr));

		// ソフトウェアアダプタを除外
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			Logger::Log(StringUtils::ConvertString(
				std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter_ = nullptr;
	}

	if (useAdapter_ == nullptr) {
		Logger::Log("Failed SelectAdapter : No hardware adapter found\n");
	}

	assert(useAdapter_ != nullptr);
}

void DirectXManager::CreateDevice() {
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

	for (size_t i = 0; i < _countof(featureLevels); i++) {
		HRESULT hr = D3D12CreateDevice(
			useAdapter_, featureLevels[i], IID_PPV_ARGS(&device_));

		if (SUCCEEDED(hr)) {
			Logger::Log(std::format(
				"FeatureLevel:{}\n", featureLevelStrings[i]));
			break;
		}
	}

	if (device_ == nullptr) {
		Logger::Log("Failed CreateDevice : No supported feature level\n");
	}

	assert(device_ != nullptr);
}

void DirectXManager::CreateCommandQueue() {

	// コマンドキューの作成
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	HRESULT hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandQueue\n");
	}
	assert(SUCCEEDED(hr));

	// コマンドアロケータの作成
	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandAllocator\n");
	}
	assert(SUCCEEDED(hr));

	// コマンドリストの作成
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_, nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandList\n");
	}
	assert(SUCCEEDED(hr));

}

void DirectXManager::CreateSwapChain(HWND hwnd, int32_t width, int32_t height) {

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	swapChainDesc.Width = width;//幅の指定
	swapChainDesc.Height = height;//高さの指定
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色の指定
	swapChainDesc.SampleDesc.Count = 1;//マルチサンプルの指定
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//描画ターゲットとして使用することを指定
	swapChainDesc.BufferCount = 2;//バッファ数の指定
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//モニターを移したら中身を破棄
	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateSwapChain\n");
	}

	assert(SUCCEEDED(hr));
}

void DirectXManager::CreateDescriptorHeap() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NumDescriptors = 2;
	HRESULT hr = device_->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDescriptorHeap\n");
	}
	assert(SUCCEEDED(hr));


}

void DirectXManager::GetSwapChainResources() {

	HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
	if (FAILED(hr)) {
		Logger::Log("Failed GetBuffer(0)\n");
	}
	assert(SUCCEEDED(hr));

	hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
	if (FAILED(hr)) {
		Logger::Log("Failed GetBuffer(1)\n");
	}
	assert(SUCCEEDED(hr));
}

void DirectXManager::CreateRTV() {
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvHandles_[0] = rtvStartHandle;
	device_->CreateRenderTargetView(swapChainResources_[0], &rtvDesc, rtvHandles_[0]);
	rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	device_->CreateRenderTargetView(swapChainResources_[1], &rtvDesc, rtvHandles_[1]);
}

void DirectXManager::beginFrame() {
	backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
	BeginTransitionBarrier();
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, nullptr);
	float clearColor[] = { 0.1f,0.25f,0.5f,0.1f };
	commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex_], clearColor, 0, nullptr);

}

void DirectXManager::endFrame() {

	EndTransitionBarrier();

	HRESULT hr = commandList_->Close();
	if (FAILED(hr)) {
		Logger::Log("Failed Close CommandList\n");
	}
	assert(SUCCEEDED(hr));

	ID3D12CommandList* commandLists[] = { commandList_ };
	commandQueue_->ExecuteCommandLists(1, commandLists);
	swapChain_->Present(1, 0);
	fenceValue_++;
	commandQueue_->Signal(fence_, fenceValue_);
	if (fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	hr = commandAllocator_->Reset();
	if (FAILED(hr)) {
		Logger::Log("Failed Reset CommandAllocator\n");
	}
	assert(SUCCEEDED(hr));

	hr = commandList_->Reset(commandAllocator_, nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed Reset CommandList\n");
	}
	assert(SUCCEEDED(hr));

}

void DirectXManager::BeginTransitionBarrier() {
	barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier_.Transition.pResource = swapChainResources_[backBufferIndex_];
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList_->ResourceBarrier(1, &barrier_);
}

void DirectXManager::EndTransitionBarrier() {
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList_->ResourceBarrier(1, &barrier_);
}

void DirectXManager::CreateFence() {
	HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateFence\n");
	}
	assert(SUCCEEDED(hr));

	fenceEvent_ = CreateEventA(NULL, FALSE, FALSE, NULL);

	if (fenceEvent_ == nullptr) {
		Logger::Log("Failed CreateEvent\n");
	}
	assert(fenceEvent_ != nullptr);
}

void DirectXManager::Finalize() {
	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12,DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}
	
}