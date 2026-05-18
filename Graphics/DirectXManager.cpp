#include "DirectXManager.h"
#include "../Utils/Logger.h"
#include "../Utils/StringUtils.h"
#include "../Math/MatrixMath.h"
#include "../Math/TransformMath.h"
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

	LoadTextureResource("Resources/texture.png", "Resources/monsterBall.png");
	CreateDescriptorRange();
	CreateStaticSamplers();

	// Primitive 描画の初期化
	InitializeDXC();
	CreatePSO();

	CreateVertexResource();
	CreateMaterialResource();
	CreateTransformationMatrix();

	CreateVertexSpriteResource();
	SetVertexSpriteResource();

	CreateVertexTransformMatrixResource();

	SetDescriptorSizes();
	GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV_, 0);


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

	// マッピング解除
	if (wvpResource_) {
		wvpResource_->Unmap(0, nullptr);
		wvpData_ = nullptr;
		wvpResource_.Reset();
	}

	// テクスチャのリソース解放
	if (isTextureLoaded_) {
		textureResource_.Reset();
		depthStencilResource_.Reset();
		intermediateResource_.Reset();
		isTextureLoaded_ = false;
	}

	// 描画関連リソース解放
	vertexResource_.Reset();
	materialResource_.Reset();
	vertexResourceSprite_.Reset();
	transformationMatrixResourceSprite_.Reset();
	vertexResourceSphere_.Reset();		

	graphicsPipelineState_.Reset();
	rootSignature_.Reset();

	vertexShaderBlob_.Reset();
	pixelShaderBlob_.Reset();
	signatureBlob_.Reset();
	errorBlob_.Reset();

	includeHandler_.Reset();
	dxcCompiler_.Reset();
	dxcUtils_.Reset();

	for (auto& res : swapChainResources_) { res.Reset(); }

	rtvDescriptorHeap_.Reset();
	srvDescriptorHeap_.Reset();
	dsvDescriptorHeap_.Reset();

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

//===========================================
//スワップチェーンの作成
//===========================================
void DirectXManager::CreateSwapChain(HWND hwnd) {
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
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
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
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));

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

void DirectXManager::LoadTextureResource(const std::string& filePath1, const std::string& filePath2) {
	DirectX::ScratchImage mipImages = LoadTexture(filePath1);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	textureResource_ = CreateTextureResource(metadata);
	depthStencilResource_ = CreateDepthStencilTextureResource(windowWidth_, windowHeight_);
	intermediateResource_ = UploadTextureData(textureResource_, mipImages);

	DirectX::ScratchImage mipImages2 = LoadTexture(filePath2);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	textureResource2_ = CreateTextureResource(metadata2);
	intermediateResource2_ = UploadTextureData(textureResource2_, mipImages2);

	WaitForGPUCompletion();
	CreateShaderResourceView(metadata, metadata2, textureResource_, textureResource2_);
	DepthShaderResourceView();

	isTextureLoaded_ = true;
	Logger::Log("Texture loaded successfully\n");
}

void DirectXManager::CreateShaderResourceView(const DirectX::TexMetadata& metadata,const DirectX::TexMetadata& metadata2, ComPtr<ID3D12Resource> textureResource, ComPtr<ID3D12Resource> textureResource2) {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, 1);

	UINT incrementSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleCPU.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	device_->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	textureSrvHandle_ = textureSrvHandleGPU;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap_.Get(), descriptorSizeSRV_, 2);
	device_->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);
	textureSrvHandle2_ = textureSrvHandleGPU2;
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

//==================================================================
//DXC関連 (Unified from PrimitiveRenderer)
//==================================================================
void DirectXManager::InitializeDXC() {
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	if (FAILED(hr)) {
		Logger::Log("Failed DxcCreateInstance for IDxcUtils\n");
	}
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	if (FAILED(hr)) {
		Logger::Log("Failed DxcCreateInstance for IDxcCompiler3\n");
	}
	assert(SUCCEEDED(hr));

	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	if (FAILED(hr)) {
		Logger::Log("Failed CreateDefaultIncludeHandler\n");
	}
	assert(SUCCEEDED(hr));
}

IDxcBlob* DirectXManager::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
	IDxcBlobEncoding* shaderSource = nullptr;
	IDxcResult* shaderResult = nullptr;
	LoadHLSLFile(filePath, profile, shaderSource);
	ExecuteCompile(filePath, profile, shaderSource, shaderResult);
	LogCompileErrors(shaderResult);

	IDxcBlob* blob = GetShaderBlob(filePath, profile, shaderResult);

	if (shaderResult) { shaderResult->Release(); }
	if (shaderSource) { shaderSource->Release(); }

	return blob;
}

void DirectXManager::LoadHLSLFile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource) {
	Logger::Log(StringUtils::ConvertString(std::format(L"Begin CompileShader, path: {}, profile: {}\n", filePath, profile)));
	HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);

	if (FAILED(hr)) {
		Logger::Log("Failed LoadFile\n");
	}
	assert(SUCCEEDED(hr));

	shaderSourceBuffer_.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer_.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer_.Encoding = DXC_CP_UTF8;
}

void DirectXManager::ExecuteCompile(const std::wstring& filePath, const wchar_t* profile, IDxcBlobEncoding*& shaderSource, IDxcResult*& shaderResult) {
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi",L"-Qembed_debug",
		L"-Od",
		L"-Zpr",
	};

	HRESULT hr = dxcCompiler_->Compile(
		&shaderSourceBuffer_,
		arguments,
		_countof(arguments),
		includeHandler_.Get(),
		IID_PPV_ARGS(&shaderResult)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed Compile\n");
	}
	assert(SUCCEEDED(hr));
}

void DirectXManager::LogCompileErrors(IDxcResult* shaderResult) {
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Logger::Log(StringUtils::ConvertString(std::format("Shader Compile Error: {}\n", shaderError->GetStringPointer())));
		assert(false);
	}
	if (shaderError) {
		shaderError->Release();
	}
}

IDxcBlob* DirectXManager::GetShaderBlob(const std::wstring& filePath, const wchar_t* profile, IDxcResult* shaderResult) {
	IDxcBlob* shaderBlob = nullptr;
	HRESULT hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

	if (FAILED(hr)) {
		Logger::Log("Failed GetOutput for shader blob\n");
	}
	assert(SUCCEEDED(hr));

	Logger::Log(StringUtils::ConvertString(std::format(L"Complete CompileShader, path: {}, profile: {}\n", filePath, profile)));
	return shaderBlob;
}

//==================================================================
// PSO関連 (Unified from PrimitiveRenderer)
//==================================================================
void DirectXManager::CreatePSO() {
	CreateRootSignature();
	InputLayout();
	BlendState();
	RasterizerState();
	VertexShader();
	PixelShader();
	DepthStencilState();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc_;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc_;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc_;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc_;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	HRESULT hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateGraphicsPipelineState\n");
	}
	assert(SUCCEEDED(hr));
}

void DirectXManager::CreateRootSignature() {
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange_;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = DESCRIPTOR_RANGE_COUNT;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	descriptionRootSignature.pStaticSamplers = staticSamplers_;
	descriptionRootSignature.NumStaticSamplers = STATIC_SAMPLER_COUNT;

	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		Logger::Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
	}
	assert(SUCCEEDED(hr));

	hr = device_->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	if (FAILED(hr)) {
		Logger::Log("Failed CreateRootSignature\n");
	}
	assert(SUCCEEDED(hr));
}

void DirectXManager::InputLayout() {
	inputElementDescs_[0].SemanticName = "POSITION";
	inputElementDescs_[0].SemanticIndex = 0;
	inputElementDescs_[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs_[1].SemanticName = "TEXCOORD";
	inputElementDescs_[1].SemanticIndex = 0;
	inputElementDescs_[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs_[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputLayoutDesc_.pInputElementDescs = inputElementDescs_;
	inputLayoutDesc_.NumElements = _countof(inputElementDescs_);
}

void DirectXManager::BlendState() {
	blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
}

void DirectXManager::RasterizerState() {
	rasterizerDesc_.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;
}

void DirectXManager::VertexShader() {
	vertexShaderBlob_.Attach(CompileShader(L"Object3D.VS.hlsl", L"vs_6_0"));
	assert(vertexShaderBlob_ != nullptr);
}

void DirectXManager::PixelShader() {
	pixelShaderBlob_.Attach(CompileShader(L"Object3D.PS.hlsl", L"ps_6_0"));
	assert(pixelShaderBlob_ != nullptr);
}

void DirectXManager::DepthStencilState() {
	depthStencilDesc_.DepthEnable = true;
	depthStencilDesc_.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

//==================================================================
//描画リソース作成 
//==================================================================
void DirectXManager::CreateVertexResource() {
	vertexResource_ = CreateBufferResource(sizeof(VertexData) * 6);
	CreateVertexBufferView();
	WriteVertexResource();
}

void DirectXManager::CreateVertexBufferView() {
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void DirectXManager::WriteVertexResource() {
	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	vertexData[0].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[0].texcoord = Vector2(0.0f, 1.0f);

	vertexData[1].position = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
	vertexData[1].texcoord = Vector2(0.5f, 0.0f);

	vertexData[2].position = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[2].texcoord = Vector2(1.0f, 1.0f);

	vertexData[3].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[3].texcoord = Vector2(0.0f, 1.0f);

	vertexData[4].position = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	vertexData[4].texcoord = Vector2(0.5f, 0.0f);

	vertexData[5].position = Vector4(0.5f, -0.5f, -0.5f, 1.0f);
	vertexData[5].texcoord = Vector2(1.0f, 1.0f);

	vertexResource_->Unmap(0, nullptr);
}

void DirectXManager::CreateMaterialResource() {
	materialResource_ = CreateBufferResource(sizeof(Vector4));
	Vector4* materialData = nullptr;
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialResource_->Unmap(0, nullptr);
}

void DirectXManager::CreateTransformationMatrix() {
	wvpResource_ = CreateBufferResource(sizeof(Matrix4x4));
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	*wvpData_ = MatrixMath::Identity();
}

void DirectXManager::CreateVertexSpriteResource() {
	vertexResourceSprite_ = CreateBufferResource(sizeof(VertexData) * 6);
	vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
	vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);

}

void DirectXManager::SetVertexSpriteResource() {
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	vertexDataSprite[0].position = Vector4(-0.5f, 360.0f, 0.0f, 1.0f);
	vertexDataSprite[0].texcoord = Vector2(0.0f, 1.0f);

	vertexDataSprite[1].position = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	vertexDataSprite[1].texcoord = Vector2(0.0f, 0.0f);
	
	vertexDataSprite[2].position = Vector4(640.0f, 360.0f, 0.0f, 1.0f);
	vertexDataSprite[2].texcoord = Vector2(1.0f, 1.0f);
	
	vertexDataSprite[3].position = Vector4(-0.0f, -0.0f, 0.0f, 1.0f);
	vertexDataSprite[3].texcoord = Vector2(0.0f, 0.0f);
	
	vertexDataSprite[4].position = Vector4(640.0f, 0.0f, 0.0f, 1.0f);
	vertexDataSprite[4].texcoord = Vector2(1.0f, 0.0f);
	
	vertexDataSprite[5].position = Vector4(640.0f, 360.0f, 0.0f, 1.0f);
	vertexDataSprite[5].texcoord = Vector2(1.0f, 1.0f);
	
	vertexResourceSprite_->Unmap(0, nullptr);
}

void DirectXManager::CreateVertexTransformMatrixResource() {
	transformationMatrixResourceSprite_ = CreateBufferResource(sizeof(Matrix4x4));
	transformationMatrixResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite_));
	*transformationMatrixDataSprite_ = MatrixMath::Identity();


}


//==================================================================
//描画関連 (Unified from PrimitiveRenderer)
//==================================================================
void DirectXManager::DrawTriangleRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform) {
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	*wvpData_ = worldMatrix * view * projection;

	ViewportScissorRect(windowWidth_, windowHeight_);
	SetPipelineCommands();
	RecordDrawCommands();
}

void DirectXManager::DrawSpriteRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform) {
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	*transformationMatrixDataSprite_ = worldMatrix * view * projection;
	ViewportScissorRect(windowWidth_, windowHeight_);
	SetPipelineCommands();
	RecordDrawCommands();
}
void DirectXManager::CreateDrawSphereResource(const SphereData& sphereData, const Matrix4x4& view, const Matrix4x4& projection,const Transform& transform) {
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	*wvpData_ = worldMatrix * view * projection;
	const uint32_t Ksubdivision = 50;
	const float kLonEvery = DirectX::XM_2PI / Ksubdivision;
	const float kLatEvery = DirectX::XM_PI / Ksubdivision;

   // Sphere uses a dedicated vertex buffer. Without this, vertexData remains nullptr and will crash on write.
	sphereVertexCount_ = Ksubdivision * Ksubdivision * 6;
	const size_t bufferSize = sizeof(VertexData) * static_cast<size_t>(sphereVertexCount_);
	vertexResourceSphere_ = CreateBufferResource(bufferSize);
	vertexBufferViewSphere_.BufferLocation = vertexResourceSphere_->GetGPUVirtualAddress();
	vertexBufferViewSphere_.SizeInBytes = static_cast<UINT>(bufferSize);
	vertexBufferViewSphere_.StrideInBytes = sizeof(VertexData);

	VertexData* vertexData = nullptr;
	HRESULT hr = vertexResourceSphere_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	if (FAILED(hr) || vertexData == nullptr) {
		Logger::Log("Failed Map vertexResourceSphere_\n");
		return;
	}

	for (uint32_t latIndex = 0; latIndex < Ksubdivision; ++latIndex) {
		float lat = -DirectX::XM_PIDIV2 + latIndex * kLatEvery;

		for (uint32_t lonIndex = 0; lonIndex < Ksubdivision; ++lonIndex) {
			uint32_t start = (latIndex * Ksubdivision + lonIndex) * 6;
			float lon = lonIndex * kLonEvery;

			Vector3 a, b, c, d;

			

			a = Vector3(
				sphereData.radius * cosf(lat) * cosf(lon),
				sphereData.radius * sinf(lat),
				sphereData.radius * cosf(lat) * sinf(lon)
			);

			b = Vector3(
				sphereData.radius * cosf(lat + kLatEvery) * cosf(lon),
				sphereData.radius * sinf(lat + kLatEvery),
				sphereData.radius * cosf(lat + kLatEvery) * sinf(lon)
			);

			c = Vector3(
				sphereData.radius * cosf(lat) * cosf(lon + kLonEvery),
				sphereData.radius * sinf(lat),
				sphereData.radius * cosf(lat) * sinf(lon + kLonEvery)
			);

			d = Vector3(
				sphereData.radius * cosf(lat + kLatEvery) * cosf(lon + kLonEvery),
				sphereData.radius * sinf(lat + kLatEvery),
				sphereData.radius * cosf(lat + kLatEvery) * sinf(lon + kLonEvery)
			);

			vertexData[start + 0].position.x = a.x;
			vertexData[start + 0].position.y = a.y;
			vertexData[start + 0].position.z = a.z;
			vertexData[start + 0].position.w = 1.0f;

			vertexData[start + 1].position.x = b.x;
			vertexData[start + 1].position.y = b.y;
			vertexData[start + 1].position.z = b.z;
			vertexData[start + 1].position.w = 1.0f;

			vertexData[start + 2].position.x = c.x;
			vertexData[start + 2].position.y = c.y;
			vertexData[start + 2].position.z = c.z;
			vertexData[start + 2].position.w = 1.0f;

			vertexData[start + 3].position.x = c.x;
			vertexData[start + 3].position.y = c.y;
			vertexData[start + 3].position.z = c.z;
			vertexData[start + 3].position.w = 1.0f;

			vertexData[start + 4].position.x = b.x;
			vertexData[start + 4].position.y = b.y;
			vertexData[start + 4].position.z = b.z;
			vertexData[start + 4].position.w = 1.0f;

			vertexData[start + 5].position.x = d.x;
			vertexData[start + 5].position.y = d.y;
			vertexData[start + 5].position.z = d.z;
			vertexData[start + 5].position.w = 1.0f;

			// テクスチャ座標を正しく計算
			float u0 = lonIndex / static_cast<float>(Ksubdivision);
			float u1 = (lonIndex + 1) / static_cast<float>(Ksubdivision);
			float v0 = 1.0f - latIndex / static_cast<float>(Ksubdivision);
			float v1 = 1.0f - (latIndex + 1) / static_cast<float>(Ksubdivision);

			// 各頂点に異なるUV座標を割り当て
			vertexData[start + 0].texcoord = { u0, v0 };  // a
			vertexData[start + 1].texcoord = { u0, v1 };  // b
			vertexData[start + 2].texcoord = { u1, v0 };  // c
			vertexData[start + 3].texcoord = { u1, v0 };  // c
			vertexData[start + 4].texcoord = { u0, v1 };  // b
			vertexData[start + 5].texcoord = { u1, v1 };  // d
		}
	}

	vertexResourceSphere_->Unmap(0, nullptr);

	ViewportScissorRect(windowWidth_, windowHeight_);
	SetPipelineCommands();
	commandList_->IASetVertexBuffers(0, 1, &vertexBufferViewSphere_);
	commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList_->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList_->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	commandList_->SetGraphicsRootDescriptorTable(2, textureSrvHandle2_);
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, &dsvHandle_);
	commandList_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList_->DrawInstanced(sphereVertexCount_, 1, 0, 0);
}
void DirectXManager::ViewportScissorRect(int32_t width, int32_t height) {
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
	scissorRect_.left = 0;
	scissorRect_.right = width;
	scissorRect_.top = 0;
	scissorRect_.bottom = height;
}

void DirectXManager::SetPipelineCommands() {
	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
	commandList_->SetGraphicsRootSignature(rootSignature_.Get());
	commandList_->SetPipelineState(graphicsPipelineState_.Get());
}

void DirectXManager::RecordDrawCommands() {
	commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList_->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList_->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
	commandList_->SetGraphicsRootDescriptorTable(2, textureSrvHandle2_);
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, &dsvHandle_);
	commandList_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList_->DrawInstanced(6, 1, 0, 0);
	commandList_->IASetVertexBuffers(0, 1, &vertexBufferViewSprite_);
	commandList_->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite_->GetGPUVirtualAddress());
	commandList_->DrawInstanced(6, 1, 0, 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXManager::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}


D3D12_GPU_DESCRIPTOR_HANDLE DirectXManager::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

void DirectXManager::SetDescriptorSizes() {
	const uint32_t descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}