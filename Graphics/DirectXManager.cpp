#include "DirectXManager.h"
#include "../Utils/Logger.h"
#include "../Utils/StringUtils.h"
#include <cassert>
#include <format>

DirectXManager::~DirectXManager() {
	Finalize();
}
//===========================================
//ライフサイクル
//===========================================
void DirectXManager::Initialize(HWND hwnd, int32_t width, int32_t height) {

	windowWidth_ = width;
	windowHeight_ = height;
	if (initialized_) {
		Logger::Log("Already initialized\n");
		return;
	}

	InitializeCOM();
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

	LoadTextureResource("Resources/texture.png");

	CreateDescriptorRange();
	CreateStaticSamplers();
	Logger::Log("Complete Initialize DirectXManager\n");

	initialized_ = true;
}

void DirectXManager::Finalize() {
	if (!initialized_) {
		return;
	}
	initialized_ = false;

	// GPU処理完了を待つ
	if (commandQueue_ && commandAllocator_ && commandList_ && fence_ && fenceEvent_) {
		WaitForGPUCompletion();
		Logger::Log("Wait for GPU completion in Finalize\n");
	}

	// テクスチャのリソース解放
	if (isTextureLoaded_) {
		textureResource_.Reset();
		depthStencilResource_.Reset();
		intermediateResource_.Reset();
		isTextureLoaded_ = false;
	}

	for (auto& res : swapChainResources_) { res.Reset(); }

	rtvDescriptorHeap_.Reset();
	srvDescriptorHeap_.Reset();

	swapChain_.Reset();
	commandList_.Reset();
	commandAllocator_.Reset();
	commandQueue_.Reset();

	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
	fence_.Reset();

	device_.Reset();
	useAdapter_.Reset();
	dxgiFactory_.Reset();

	FinalizeCOM();
	initialized_ = false;
	Logger::Log("Complete Finalize DirectXManager\n");
}

//===========================================
//フレーム管理
//===========================================
void DirectXManager::BeginFrame() {
	backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
	BeginTransitionBarrier();
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, nullptr);
	float clearColor[] = { 0.1f,0.25f,0.5f,0.1f };
	commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex_], clearColor, 0, nullptr);
	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
	commandList_->SetDescriptorHeaps(1, descriptorHeaps);
}

void DirectXManager::EndFrame() {

	EndTransitionBarrier();

	HRESULT hr = commandList_->Close();
	if (FAILED(hr)) {
		Logger::Log("Failed Close CommandList\n");
	}
	assert(SUCCEEDED(hr));

	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(1, commandLists);
	swapChain_->Present(1, 0);
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
	if (fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	hr = commandAllocator_->Reset();
	if (FAILED(hr)) {
		Logger::Log("Failed Reset CommandAllocator\n");
	}
	assert(SUCCEEDED(hr));

	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed Reset CommandList\n");
	}
	assert(SUCCEEDED(hr));

}

//===========================================
//COMの初期化
//===========================================
void DirectXManager::InitializeCOM() {
	if (comInitialized_) {
		Logger::Log("Com already initialized\n");
		return;
	}

	HRESULT hr = CoInitializeEx(0, COINITBASE_MULTITHREADED);
	if (FAILED(hr)) {
		Logger::Log("Failed CoInitializeEx\n");
	}

	assert(SUCCEEDED(hr));
	comInitialized_ = true;
}

void DirectXManager::FinalizeCOM() {
	if (!comInitialized_) {
		Logger::Log("Com is not initialized\n");
		return;
	}

	CoUninitialize();
	comInitialized_ = false;
}

//===========================================
//Factory & Deviceの作成
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
			useAdapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));

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

//===========================================
//コマンドキューとコマンドリストの作成
//===========================================
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
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandList\n");
	}
	assert(SUCCEEDED(hr));

}

//===========================================
//スワップチェーンの作成
//===========================================
void DirectXManager::CreateSwapChain(HWND hwnd) {

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	swapChainDesc.Width = windowWidth_;//幅の指定
	swapChainDesc.Height = windowHeight_;//高さの指定
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色の指定
	swapChainDesc.SampleDesc.Count = 1;//マルチサンプルの指定
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//描画ターゲットとして使用することを指定
	swapChainDesc.BufferCount = 2;//バッファ数の指定
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//モニターを移したら中身を破棄
	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateSwapChain\n");
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

//===========================================
//Descriptor Heapの作成
//===========================================
ComPtr<ID3D12DescriptorHeap> DirectXManager::CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
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
	rtvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
}

void DirectXManager::CreateSRVDescriptorHeap() {
	srvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
}

void DirectXManager::CreateDSVDescriptorHeap() {
	dsvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}
//===========================================
//Render Target Viewの作成
//===========================================
void DirectXManager::CreateRTV() {
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvHandles_[0] = rtvStartHandle;
	device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);
	rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);
}

//===========================================
//同期オブジェクトの作成
//=========================================== 
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

void DirectXManager::WaitForGPUCompletion() {
	if (!commandQueue_ || !commandAllocator_ || !commandList_ || !fence_ || !fenceEvent_) {
		Logger::Log("Skip WaitForGPUCompletion : DirectX resources are not initialized\n");
		return;
	}

	HRESULT hr = commandList_->Close();
	// すでにClose済みの場合 E_FAIL になることがあるため許容する
	if (FAILED(hr) && hr != E_FAIL) {
		Logger::Log("Failed Close CommandList\n");
		return;
	}

	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(1, commandLists);

	fenceValue_++;
	hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
	if (FAILED(hr)) {
		Logger::Log("Failed Signal Fence\n");
		return;
	}
	if (fence_->GetCompletedValue() < fenceValue_) {
		hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		if (FAILED(hr)) {
			Logger::Log("Failed SetEventOnCompletion\n");
			return;
		}
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	hr = commandAllocator_->Reset();
	if (FAILED(hr)) {
		Logger::Log("Failed Reset CommandAllocator\n");
	}
	assert(SUCCEEDED(hr));

	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	if (FAILED(hr)) {
		Logger::Log("Failed Reset CommandList\n");
	}
	assert(SUCCEEDED(hr));

}

//===========================================
//リソース管理
//===========================================
ComPtr<ID3D12Resource> DirectXManager::CreateTextureResource(const DirectX::TexMetadata& metadata) {

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

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(&heapProperties,D3D12_HEAP_FLAG_NONE,&resourceDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));
	return resource;
}

ComPtr<ID3D12Resource> DirectXManager::CreateBufferResource(size_t sizeInBytes) {
	ComPtr<ID3D12Resource> resource = nullptr;
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC vertexBufferResourceDesc{};
	vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexBufferResourceDesc.Width = sizeInBytes;
	vertexBufferResourceDesc.Height = 1;
	vertexBufferResourceDesc.DepthOrArraySize = 1;
	vertexBufferResourceDesc.MipLevels = 1;
	vertexBufferResourceDesc.SampleDesc.Count = 1;
	vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device_->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));
	return resource;
}

[[nodiscard]]
ComPtr<ID3D12Resource> DirectXManager::UploadTextureData(ComPtr<ID3D12Resource> textureResource, const DirectX::ScratchImage& mipImages) {

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(textureResource.Get(), 0, UINT(subresources.size()));
	ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);
	UpdateSubresources(commandList_.Get(), textureResource.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = textureResource.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList_->ResourceBarrier(1, &barrier);
	return intermediateResource;
}

ComPtr<ID3D12Resource> DirectXManager::CreateDepthStencilTextureResource(int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(width);
	resourceDesc.Height = UINT(height);
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&resource));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));
	return resource;
}

//===========================================
//テクスチャロード
//===========================================
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
void DirectXManager::LoadTextureResource(const std::string& filePath) {
	DirectX::ScratchImage mipImages = LoadTexture(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	textureResource_ = CreateTextureResource(metadata);
	depthStencilResource_ = CreateDepthStencilTextureResource(windowWidth_,windowHeight_);
	intermediateResource_ = UploadTextureData(textureResource_, mipImages);

	WaitForGPUCompletion();
	CreateShaderResourceView(metadata, textureResource_);
	DepthShaderResourceView();

	isTextureLoaded_ = true;
	Logger::Log("Texture loaded successfully\n");
}
void DirectXManager::CreateShaderResourceView(const DirectX::TexMetadata& metadata, ComPtr<ID3D12Resource> textureResource) {
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

	device_->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	textureSrvHandle_ = textureSrvHandleGPU;
}

void DirectXManager::DepthShaderResourceView() {
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
		dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
}


//===========================================
//状態遷移バリア
//===========================================
void DirectXManager::BeginTransitionBarrier() {
	barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier_.Transition.pResource = swapChainResources_[backBufferIndex_].Get();
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList_->ResourceBarrier(1, &barrier_);
}
void DirectXManager::EndTransitionBarrier() {
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList_->ResourceBarrier(1, &barrier_);
}

//===========================================	
//パイプライン設定
//===========================================
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

