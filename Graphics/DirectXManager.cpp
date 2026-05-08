#include "DirectXManager.h"
#include "../Utils/Logger.h"
#include "../Utils/StringUtils.h"
#include <cassert>
#include <format>

DirectXManager::~DirectXManager() {
	Finalize();
}
//===========================================
//public
//===========================================

void DirectXManager::Initialize(HWND hwnd, int32_t width, int32_t height) {
	InitializeCOM();
	CreateFactory();
	SelectAdapter();
	CreateDevice();
	CreateCommandQueue();
	CreateSwapChain(hwnd, width, height);
	GetSwapChainResources();
	CreateRTVDescriptorHeap();
	CreateSRVDescriptorHeap();
	CreateRTV();
	CreateFence();

	LoadTextureResource("Resources/texture.png");
	CreateDescriptorRange();
	CreateStaticSamplers();
	Logger::Log("Complete Initialize DirectXManager\n");
}

void DirectXManager::BeginFrame() {
	backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
	BeginTransitionBarrier();
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, nullptr);
	float clearColor[] = { 0.1f,0.25f,0.5f,0.1f };
	commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex_], clearColor, 0, nullptr);
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_ };
	commandList_->SetDescriptorHeaps(1, descriptorHeaps);
}

void DirectXManager::EndFrame() {

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

void DirectXManager::Finalize() {

	if (textureResource_) { textureResource_->Release(); textureResource_ = nullptr; }
	if (fenceEvent_) { CloseHandle(fenceEvent_);	fenceEvent_ = nullptr; }
	if (fence_) { fence_->Release(); fence_ = nullptr; }
	if (srvDescriptorHeap_) { srvDescriptorHeap_->Release(); srvDescriptorHeap_ = nullptr; }
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
	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}
	FinalizeCOM();
}
//===========================================
//private
//===========================================


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

ID3D12DescriptorHeap* DirectXManager::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));

	if (FAILED(hr)) {
		Logger::Log("Failed CreateDescriptorHeap\n");
	}

	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

void DirectXManager::CreateRTVDescriptorHeap() {
	rtvDescriptorHeap_ = CreateDescriptorHeap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
}

void DirectXManager::CreateSRVDescriptorHeap() {
	srvDescriptorHeap_ = CreateDescriptorHeap(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
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
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvHandles_[0] = rtvStartHandle;
	device_->CreateRenderTargetView(swapChainResources_[0], &rtvDesc, rtvHandles_[0]);
	rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	device_->CreateRenderTargetView(swapChainResources_[1], &rtvDesc, rtvHandles_[1]);
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


void DirectXManager::InitializeCOM() {
	HRESULT hr = CoInitializeEx(0, COINITBASE_MULTITHREADED);
	if (FAILED(hr)) {
		Logger::Log("Failed CoInitializeEx\n");
	}
	assert(SUCCEEDED(hr));
}

void DirectXManager::FinalizeCOM() {
	CoUninitialize();
}

DirectX::ScratchImage DirectXManager::LoadTexture(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = StringUtils::ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
	if (FAILED(hr)) {
		Logger::Log(std::format("Failed LoadFromWICFile : {}\n", filePath));
	}
	assert(SUCCEEDED(hr));

	DirectX::ScratchImage mipImage{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImage);
	if (FAILED(hr)) {
		Logger::Log(std::format("Failed GenerateMipMaps : {}\n", filePath));
	}
	assert(SUCCEEDED(hr));

	return mipImage;

}

ID3D12Resource* DirectXManager::CreateTextureResource(const DirectX::TexMetadata& metadata) {

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	ID3D12Resource* textureResource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureResource));

	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));
	return textureResource;
}

void DirectXManager::UploadTextureData(ID3D12Resource* textureResource, const DirectX::ScratchImage& mipImages) {
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels;++mipLevel) {
		const DirectX::Image* image = mipImages.GetImage(mipLevel, 0, 0);

		HRESULT hr = textureResource->WriteToSubresource(
			UINT(mipLevel),
			nullptr,
			image->pixels,
			UINT(image->rowPitch),
			UINT(image->slicePitch));
		if (FAILED(hr)) {
			Logger::Log(std::format("Failed WriteToSubresource : mipLevel {}\n", mipLevel));
		}
		assert(SUCCEEDED(hr));
	}
}

void DirectXManager::CreateTextureFromFile(const std::string& filePath) {
	DirectX::ScratchImage mipImages = LoadTexture(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	ID3D12Resource* textureResource = CreateTextureResource(metadata);
	UploadTextureData(textureResource, mipImages);
}

void DirectXManager::CreateShaderResourceView(const DirectX::TexMetadata& metadata, ID3D12Resource* textureResource) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart();

	UINT incrementSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleCPU.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device_->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);
	textureSrvHandle_ = textureSrvHandleGPU;
}

void DirectXManager::LoadTextureResource(const std::string& filePath) {
	DirectX::ScratchImage mipImages = LoadTexture(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	textureResource_ = CreateTextureResource(metadata);
	UploadTextureData(textureResource_, mipImages);
	CreateShaderResourceView(metadata, textureResource_);
}

void DirectXManager::CreateDescriptorRange() {
	descriptorRange_[0].BaseShaderRegister = 0;
	descriptorRange_[0].NumDescriptors = 1;
	descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
}

void DirectXManager::CreateStaticSamplers() {
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers_[0].ShaderRegister = 0;
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

}
