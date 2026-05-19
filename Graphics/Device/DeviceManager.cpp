#include "DeviceManager.h"
#include "../../Utils/Logger.h"
#include "../../Utils/StringUtils.h"
#include <cassert>
#include <format>
void DeviceManager::Initialize(HWND hwnd, int32_t width, int32_t height) {
	windowWidth_ = width;
	windowHeight_ = height;
	CreateFactory();
	SelectAdapter();
	CreateDevice();
	CreateCommandQueue();
	
	CreateSwapChain(hwnd);
	GetSwapChainResources();

	CreateRTVDescriptorHeap();
	CreateSRVDescriptorHeap();
	CreateDSVDescriptorHeap();
	CreateRTV();

	CreateFence();
	SetDescriptorSizes();
}

//===========================================
//Factory & Deviceの作成
//===========================================
void DeviceManager::CreateFactory() {
	//DXGIファクトリの作成
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDXGIFactory\n");
	}

	assert(SUCCEEDED(hr));
}

void DeviceManager::SelectAdapter() {
	//GPUアダプタの列挙と選択
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) != DXGI_ERROR_NOT_FOUND; ++i) {

		DXGI_ADAPTER_DESC3 adapterDesc{};
		// アダプタを取得
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

void DeviceManager::CreateDevice() {
	// デバイスの作成と機能レベルの確認
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };

	// 最も高い機能レベルから順にデバイスの作成を試みる
	for (size_t i = 0; i < _countof(featureLevels); i++) {
		HRESULT hr = D3D12CreateDevice(
			useAdapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));

		if (SUCCEEDED(hr)) {
			Logger::Log(std::format("Created device with feature level {}\n", featureLevelStrings[i]));
			break;
		}
	}

	if (device_ == nullptr) {
		Logger::Log("Failed CreateDevice : No supported feature level\n");
	}
	assert(device_ != nullptr);
}

//===========================================
//コマンドキューの作成
//===========================================
void DeviceManager::CreateCommandQueue() {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	// コマンドキューのタイプを指定
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
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandList\n");
	}
	assert(SUCCEEDED(hr));
}

//===========================================
//スワップチェーンの作成
//===========================================
void DeviceManager::CreateSwapChain(HWND hwnd) {
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	// スワップチェーンの設定
	swapChainDesc.Width = windowWidth_;
	swapChainDesc.Height = windowHeight_;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	
	// ウィンドウにスワップチェーンを作成
	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
		commandQueue_.Get(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));

	if (FAILED(hr)) {
		Logger::Log("Failed CreateSwapChainForHwnd\n");
	}
	assert(SUCCEEDED(hr));
}

void DeviceManager::GetSwapChainResources() {
	for (UINT i = 0; i < 2; i++) {
		// スワップチェーンのリソースを取得
		HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
		if (FAILED(hr)) {
			Logger::Log(std::format("Failed GetBuffer for swap chain resource {}, index: {}\n", i, i));
		}
		assert(SUCCEEDED(hr));
	}
}

//===========================================
//同期のためのフェンスの作成
//===========================================
void DeviceManager::CreateFence() {
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

//===========================================
//Descriptorサイズの取得
//===========================================
void DeviceManager::SetDescriptorSizes() {
	// デバイスからディスクリプタサイズを取得
	descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}