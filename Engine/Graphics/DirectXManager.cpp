#include "DirectXManager.h"
#include "../Utils/Logger.h"
#include "../Utils/StringUtils.h"
#include "../../Math/MatrixMath.h"
#include "../../Math/TransformMath.h"
#include <cassert>
#include <algorithm>
#include <format>
#include <cstring>

namespace {
	constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment) {
		return (value + (alignment - 1)) & ~(alignment - 1);
	}
}

DirectXManager::~DirectXManager() {
	Finalize();
}

uint32_t DirectXManager::AllocateWvpIndex() {
	if (wvpAllocatedCount_ >= kMaxWvpCount) {
		Logger::Log("WVP pool is full (kMaxWvpCount reached)\n");
		return UINT32_MAX;
	}
	return wvpAllocatedCount_++;
}

void DirectXManager::SetWvpMatrix(uint32_t index, const Matrix4x4& matrix) {
	if (!wvpMappedData_ || !wvpResource_) {
		return;
	}
	if (index >= kMaxWvpCount) {
		return;
	}
	std::memcpy(wvpMappedData_ + static_cast<size_t>(index) * wvpStride_, &matrix, sizeof(Matrix4x4));
}

D3D12_GPU_VIRTUAL_ADDRESS DirectXManager::GetWvpGpuAddress(uint32_t index) const {
	if (!wvpResource_ || index >= kMaxWvpCount) {
		return 0;
	}
	return wvpResource_->GetGPUVirtualAddress() + static_cast<UINT64>(index) * wvpStride_;
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
	descriptorHeaps_.Initialize(device_.Get());
	descriptorHeaps_.CreateRTV(device_.Get(), swapChainResources_[0].Get(), swapChainResources_[1].Get());
	CreateFence();

	InitializeTexture();
	shaderCompiler_.InitializeDXC();
	// Primitive 描画の初期化
	pipeline_.Initialize(device_.Get(), &shaderCompiler_);
	linePipeline_.Initialize(device_.Get(), &shaderCompiler_);

	triangle_ = std::make_unique<Triangle>();
	triangle_->Initialize(device_.Get(), &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	line_ = std::make_unique<Line>();
	line_->Initialize(device_.Get(), &textureManager_, linePipeline_.GetRootSignature(), linePipeline_.GetPipelineState());
	
	CreateVertexResource();
	CreateMaterialResource();
	CreateTransformationMatrix();

	CreateVertexSpriteResource();
	SetVertexSpriteResource();

	CreateVertexTransformMatrixResource();

	ViewportScissorRect(windowWidth_, windowHeight_);
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
		wvpMappedData_ = nullptr;
		wvpResource_.Reset();
	}



	// 描画関連リソース解放
	materialResource_.Reset();
	vertexResourceSprite_.Reset();
	transformationMatrixResourceSprite_.Reset();
	vertexResourceSphere_.Reset();

	line_.reset();
	triangle_.reset();
	

	textureManager_.Finalize();

	for (auto& res : swapChainResources_) { res.Reset(); }

	descriptorHeaps_.Finalize();

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
	currentTriangleWvpIndex_ = 0; // フレームごとにインデックスをリセット
	currentLineWvpIndex_ = 0; // フレームごとにインデックスをリセット

	backBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
	BeginTransitionBarrier();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = descriptorHeaps_.GetRTVHandle(backBufferIndex_);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = descriptorHeaps_.GetDSVHandle();
	commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
	float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };
	commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeaps_.GetSRVDescriptorHeap() };
	commandList_->SetDescriptorHeaps(1, descriptorHeaps);
	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);

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

//==================================================================
//描画リソース作成 
//==================================================================
void DirectXManager::CreateVertexResource() {
	vertexResource_ = textureManager_.CreateBufferResource(sizeof(VertexData) * 6);
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
	materialResource_ = textureManager_.CreateBufferResource(sizeof(Vector4));
	Vector4* materialData = nullptr;
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialResource_->Unmap(0, nullptr);
}

void DirectXManager::CreateTransformationMatrix() {
	wvpStride_ = AlignUp(static_cast<uint32_t>(sizeof(Matrix4x4)), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	wvpResource_ = textureManager_.CreateBufferResource(static_cast<size_t>(wvpStride_) * kMaxWvpCount);
	wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));

	wvpAllocatedCount_ = 0;
	AllocateWvpIndex(); // triangle
	AllocateWvpIndex(); // sphere
	SetWvpMatrix(kTriangleWvpIndex, MatrixMath::Identity());
	SetWvpMatrix(kSphereWvpIndex, MatrixMath::Identity());
}

void DirectXManager::CreateVertexSpriteResource() {
	vertexResourceSprite_ = textureManager_.CreateBufferResource(sizeof(VertexData) * 6);
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
	transformationMatrixResourceSprite_ = textureManager_.CreateBufferResource(sizeof(Matrix4x4));
	transformationMatrixResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite_));
	*transformationMatrixDataSprite_ = MatrixMath::Identity();
}


//==================================================================
//描画関連 (Unified from PrimitiveRenderer)
//==================================================================
void DirectXManager::DrawTriangleRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, const Vector4& color, TextureID textureID) {
	if (!triangle_) {
		Logger::Log("Triangle is not initialized\n");
		return;
	}

	// ワールド行列を計算
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);

	// フレームごとのインデックスを自動割り当て（バッファ上限チェック）
	if (currentTriangleWvpIndex_ >= Triangle::kMaxInstanceCount) {
		Logger::Log("DrawTriangleRender: WVP buffer full, skipping draw\n");
		return;
	}
	uint32_t wvpIndex = currentTriangleWvpIndex_++;

	// WVP 行列を計算して三角形に設定
	Matrix4x4 wvpMatrix = worldMatrix * view * projection;

	triangle_->SetColor(color, wvpIndex); // 色を設定

	triangle_->SetWvpMatrix(wvpMatrix, wvpIndex);

	// SetViewportAndScissorRect は削除（BeginFrame で既に設定済み）

	triangle_->SetPipelineCommands(commandList_.Get(), &textureManager_, textureID);

	// Triangle の Draw を呼び出し
	triangle_->Draw(commandList_.Get(), wvpIndex);
}

void DirectXManager::DrawLineRender(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& start, const Vector3& end, const Vector4& color) {
	if (!line_) {
		Logger::Log("Line is not initialized\n");
		return;
	}
	if (!line_) {
		Logger::Log("DrawLineRender : Line is not initialized\n");
		return;
	}

	uint32_t wvpIndex = currentLineWvpIndex_++;

	// ラインはワールド変換なし、VP行列のみ
	Matrix4x4 vpMatrix = view * projection;
	line_->SetWvpMatrix(vpMatrix, wvpIndex);

	// 始点・終点をセット
	line_->SetLine(start, end, wvpIndex);

	// 色をセット
	line_->SetColor(color);

	// SetViewportAndScissorRect は削除（BeginFrame で既に設定済み）

	line_->SetPipelineCommands(commandList_.Get());
	line_->Draw(commandList_.Get(), wvpIndex);
}


void DirectXManager::DrawSpriteRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform) {
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	*transformationMatrixDataSprite_ = worldMatrix * view * projection;
	ViewportScissorRect(windowWidth_, windowHeight_);
	SetPipelineCommands();
	RecordDrawCommands();
}
void DirectXManager::CreateDrawSphereResource(const SphereData& sphereData, const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, bool changeTexture, uint32_t wvpIndex) {
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	SetWvpMatrix(wvpIndex, worldMatrix * view * projection);
	const uint32_t Ksubdivision = 30;
	const float kLonEvery = DirectX::XM_2PI / Ksubdivision;
	const float kLatEvery = DirectX::XM_PI / Ksubdivision;

	// Sphere uses a dedicated vertex buffer. Without this, vertexData remains nullptr and will crash on write.
	sphereVertexCount_ = Ksubdivision * Ksubdivision * 6;
	const size_t bufferSize = sizeof(VertexData) * static_cast<size_t>(sphereVertexCount_);
	vertexResourceSphere_ = textureManager_.CreateBufferResource(bufferSize);
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
	commandList_->SetGraphicsRootConstantBufferView(1, GetWvpGpuAddress(wvpIndex));

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = changeTexture ?
		textureManager_.GetSrvGpuHandle(TextureID::Texture2) :
		textureManager_.GetSrvGpuHandle(TextureID::Texture1);

	commandList_->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

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

	if (triangle_) {
		commandList_->SetGraphicsRootSignature(triangle_->GetRootSignature());
		commandList_->SetPipelineState(triangle_->GetPipelineState());
	}

	// テクスチャ SRV ハンドルを設定
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle =
		textureManager_.GetSrvGpuHandle(TextureID::Texture3);
	commandList_->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
}
void DirectXManager::RecordDrawCommands() {
	commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList_->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = textureManager_.GetSrvGpuHandle(TextureID::Texture3);
	commandList_->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

	commandList_->IASetVertexBuffers(0, 1, &vertexBufferViewSprite_);
	commandList_->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite_->GetGPUVirtualAddress());
	commandList_->DrawInstanced(6, 1, 0, 0);
}

void DirectXManager::InitializeTexture() {
	textureManager_.SetDevice(device_.Get());
	textureManager_.SetCommandList(commandList_.Get());
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture1, "Resources/t.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture2, "Resources/s.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture3, "Resources/t.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::None, "Resources/White.png");
	textureManager_.InitializeDepthStencil(windowWidth_, windowHeight_, &descriptorHeaps_);

	WaitForGPUCompletion();

	textureManager_.CreateShaderResourceView(TextureID::Texture1, &descriptorHeaps_);
	textureManager_.CreateShaderResourceView(TextureID::Texture2, &descriptorHeaps_);
	textureManager_.CreateShaderResourceView(TextureID::Texture3, &descriptorHeaps_);
	textureManager_.CreateShaderResourceView(TextureID::None, &descriptorHeaps_);

	Logger::Log("Textures initialized successfully\n");
}


void DirectXManager::InitializeGridLines() {
	const float halfSize = 50.0f;
	const float step = 1.0f;
	uint32_t lineIndex = 0;

	for (float x = -halfSize; x <= halfSize; x += step) {
		line_->SetLine({ x, 0.0f, -halfSize }, { x, 0.0f, halfSize }, lineIndex++);
	}
	for (float z = -halfSize; z <= halfSize; z += step) {
		line_->SetLine({ -halfSize, 0.0f, z }, { halfSize, 0.0f, z }, lineIndex++);
	}
}

void DirectXManager::DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection) {
	if (!line_) {
		Logger::Log("DrawGridBatch : Line is not initialized\n");
		return;
	}
	// グリッド用は固定のwvpIndex=0を使う（毎フレーム同じスロットを上書き）
	const uint32_t kGridWvpIndex = 0;
	Matrix4x4 vpMatrix = view * projection;
	line_->SetWvpMatrix(vpMatrix, kGridWvpIndex);

	// SetViewportAndScissorRect は削除（BeginFrame で既に設定済み）

	line_->SetPipelineCommands(commandList_.Get());
	line_->DrawBatch(commandList_.Get(), 0, 202, kGridWvpIndex);
}