#include "DirectX.h"
#include "Utils/Logger.h"
#include "Utils/StringUtils.h"
#include <cassert>
#include <format>
//=============================
// public
//=============================

//DirectXのデストラクタ
DirectX::~DirectX() {}

//DirectXの初期化
void DirectX::Initialize(HWND hwnd, int32_t width, int32_t height) {
	windowWidth_ = width;
	windowHeight_ = height;

	CreateDxgiFactory();
	SelectAdapter();
	CreateDevice();
	CreateCommandQueueAndList();
	CreateSwapChain(hwnd);
	GetSwapChainResources();
	shaderCompiler_.InitializeDXC();
	pipline_.Initialize(device_.Get(), &shaderCompiler_);
	descriptorHeaps_.Initialize(device_.Get());
}
//=============================
// private
//=============================

#pragma region Factory & Deviceの作成
//DXGI Factoryの作成
void DirectX::CreateDxgiFactory() {
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDXGIFactory1\n");
	}
	assert(SUCCEEDED(hr));
}

//アダプタの選択
void DirectX::SelectAdapter() {
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

//デバイスの作成
void DirectX::CreateDevice() {
	HRESULT hr = D3D12CreateDevice(useAdapter_.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_));
	if (FAILED(hr)) {
		Logger::Log("Failed D3D12CreateDevice\n");
	}
	assert(SUCCEEDED(hr));
}
#pragma endregion

#pragma region Command Queue & Command Listの作成
//コマンドキューとコマンドリストの作成
void DirectX::CreateCommandQueueAndList() {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	HRESULT hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandQueue\n");
	}
	assert(SUCCEEDED(hr));

	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandAllocator\n");
	}
	assert(SUCCEEDED(hr));

	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandList\n");
	}
	assert(SUCCEEDED(hr));
}
#pragma endregion

#pragma region SwapChainの作成
//SwapChainの作成
void DirectX::CreateSwapChain(HWND hwnd) {
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = windowWidth_; 
	swapChainDesc.Height = windowHeight_; 
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateSwapChain\n");
	}
	assert(SUCCEEDED(hr));
}

//SwapChainのリソース取得
void DirectX::GetSwapChainResources() {
	for (UINT i = 0; i < backBufferCount_; i++) {
		HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
		if (FAILED(hr)) {
			Logger::Log("Failed GetBuffer for SwapChain\n");
		}
		assert(SUCCEEDED(hr));
	}
}
#pragma endregion