#include "DirectXManager.h"
#include "../../Utils/Logger.h"
#include "../../Utils/StringUtils.h"
#include "../../../Math/MatrixMath.h"
#include "../ResourceFactory/ResourceFactory.h"
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

void DirectXManager::Initialize(HWND hwnd, int width, int height) {
	windowWidth_ = width;
	windowHeight_ = height;
	if (initialized_) {
		Logger::Log("Already initialized\n");
		return;
	}

	// Initialize sub-systems in order
	device_.Initialize();
	command_.Initialize(device_.GetDevice());
	swapChain_.Initialize(device_.GetDxgiFactory(), device_.GetDevice(), command_.GetCommandQueue(), hwnd, width, height);

	InitializeTexture();
	shaderCompiler_.InitializeDXC();

	// Initialize pipelines
	pipeline_.Initialize(device_.GetDevice(), &shaderCompiler_);
	linePipeline_.Initialize(device_.GetDevice(), &shaderCompiler_);

	// Initialize drawable objects
	triangle_ = std::make_unique<Triangle>();
	triangle_->Initialize(device_.GetDevice(), &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	line_ = std::make_unique<Line>();
	line_->Initialize(device_.GetDevice(), &textureManager_, linePipeline_.GetRootSignature(), linePipeline_.GetPipelineState());

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(device_.GetDevice(), &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(device_.GetDevice(), &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	InitializeGridLines();

	Logger::Log("Complete Initialize Renderer\n");
	initialized_ = true;
}

void DirectXManager::BeginFrame() {
	command_.WaitForFence();

	ID3D12GraphicsCommandList* commandList = command_.GetCommandList();

	command_.ResetCommandList();
	commandList = command_.GetCommandList();

	swapChain_.BeginTransitionBarrier(commandList);
	swapChain_.SetRenderTarget(commandList);
	swapChain_.SetViewport(commandList);

	ID3D12DescriptorHeap* descriptorHeaps[] = { swapChain_.GetDescriptorHeaps()->GetSRVDescriptorHeap() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	currentTriangleWvpIndex_ = 0;
	currentLineWvpIndex_ = 0;
}

void DirectXManager::EndFrame() {
	ID3D12GraphicsCommandList* commandList = command_.GetCommandList();

	// Transition to present
	swapChain_.EndTransitionBarrier(commandList);

	HRESULT hr = commandList->Close();
	if (FAILED(hr)) {
		Logger::Log("Failed Close CommandList\n");
	}
	assert(SUCCEEDED(hr));

	command_.ExecuteCommandList();
	command_.Signal();
	swapChain_.Present();
}

void DirectXManager::Finalize() {
	if (!initialized_) {
		return;
	}
	initialized_ = false;

	// Wait for GPU to complete
	command_.Signal();
	command_.WaitForFence();

	textureManager_.Finalize();
	swapChain_.Finalize();
	device_.Finalize();

	Logger::Log("Complete Finalize Renderer\n");
}

void DirectXManager::InitializeTexture() {
	textureManager_.SetDevice(device_.GetDevice());
	textureManager_.SetCommandList(command_.GetCommandList());
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture1, "Resources/t.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture2, "Resources/s.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture3, "Resources/t.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::None, "Resources/White.png");
	textureManager_.InitializeDepthStencil(windowWidth_, windowHeight_, swapChain_.GetDescriptorHeaps());

	WaitForGPUCompletion();

	textureManager_.CreateShaderResourceView(TextureID::Texture1, swapChain_.GetDescriptorHeaps());
	textureManager_.CreateShaderResourceView(TextureID::Texture2, swapChain_.GetDescriptorHeaps());
	textureManager_.CreateShaderResourceView(TextureID::Texture3, swapChain_.GetDescriptorHeaps());
	textureManager_.CreateShaderResourceView(TextureID::None, swapChain_.GetDescriptorHeaps());

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

	triangle_->SetPipelineCommands(command_.GetCommandList(), &textureManager_, textureID);

	// Triangle の Draw を呼び出し
	triangle_->Draw(command_.GetCommandList(), wvpIndex);
}

void DirectXManager::DrawLineRender(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& start, const Vector3& end, const Vector4& color) {
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

	line_->SetPipelineCommands(command_.GetCommandList());
	line_->Draw(command_.GetCommandList(), wvpIndex);
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

	line_->SetPipelineCommands(command_.GetCommandList());
	line_->DrawBatch(command_.GetCommandList(), 0, 202, kGridWvpIndex);
}

void DirectXManager::DrawSpriteRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform) {
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	sprite_->SetWvpMatrix(worldMatrix * view * projection);
	sprite_->SetPipelineCommands(command_.GetCommandList(), &textureManager_, TextureID::Texture3);
	sprite_->Draw(command_.GetCommandList(), 0);
}

void DirectXManager::DrawSphereRender(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, TextureID textureID) {
	Matrix4x4 worldMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	sphere_->SetWvpMatrix(worldMatrix * view * projection);
	sphere_->SetPipelineCommands(command_.GetCommandList(), &textureManager_, textureID);
	sphere_->Draw(command_.GetCommandList(), 0);
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