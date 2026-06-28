#include "Renderer.h"
#include "../../Utils/Logger.h"
#include "../../../Math/MatrixMath.h"
#include "../DescriptorHeaps/DescriptorHeaps.h"

void Renderer::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorHeaps* heaps, int width, int height) {
	device_ = device;
	commandList_ = commandList;
	windowWidth_ = width;
	windowHeight_ = height;

	shaderCompiler_.InitializeDXC();
	pipeline_.Initialize(device_, &shaderCompiler_);
	linePipeline_.Initialize(device_, &shaderCompiler_);

	textureManager_.SetDevice(device_);
	textureManager_.SetCommandList(commandList_);
	textureManager_.LoadTextureResourceFromFile(TextureID::None,     "Resources/White.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture1, "Resources/t.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture2, "Resources/s.png");
	textureManager_.LoadTextureResourceFromFile(TextureID::Texture3, "Resources/t.png");
	textureManager_.InitializeDepthStencil(width, height, heaps);

	textureManager_.CreateShaderResourceView(TextureID::None,     heaps);
	textureManager_.CreateShaderResourceView(TextureID::Texture1, heaps);
	textureManager_.CreateShaderResourceView(TextureID::Texture2, heaps);
	textureManager_.CreateShaderResourceView(TextureID::Texture3, heaps);

	triangle_ = std::make_unique<Triangle>();
	triangle_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	line_ = std::make_unique<Line>();
	line_->Initialize(device_, &textureManager_, linePipeline_.GetRootSignature(), linePipeline_.GetPipelineState());

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	InitializeGridLines();

	Logger::Log("Complete Initialize Renderer\n");
}

void Renderer::Finalize() {
	textureManager_.Finalize();
}

void Renderer::ResetFrameIndex() {
	currentTriangleIndex_ = 0;
	currentLineIndex_ = 0;
}

void Renderer::InitializeGridLines() {
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

void Renderer::DrawTriangle(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, const Vector4& color, TextureID textureID) {
	if (!triangle_) {
		Logger::Log("DrawTriangle: Triangle is not initialized\n");
		return;
	}
	if (currentTriangleIndex_ >= Triangle::kMaxInstanceCount) {
		Logger::Log("DrawTriangle: WVP buffer full, skipping draw\n");
		return;
	}
	uint32_t wvpIndex = currentTriangleIndex_++;

	Matrix4x4 wvpMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation) * view * projection;
	triangle_->SetColor(color, wvpIndex);
	triangle_->SetWvpMatrix(wvpMatrix, wvpIndex);
	triangle_->SetPipelineCommands(commandList_, &textureManager_, textureID);
	triangle_->Draw(commandList_, wvpIndex);
}

void Renderer::DrawLine(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& start, const Vector3& end, const Vector4& color) {
	if (!line_) {
		Logger::Log("DrawLine: Line is not initialized\n");
		return;
	}
	uint32_t wvpIndex = currentLineIndex_++;
	line_->SetWvpMatrix(view * projection, wvpIndex);
	line_->SetLine(start, end, wvpIndex);
	line_->SetColor(color);
	line_->SetPipelineCommands(commandList_);
	line_->Draw(commandList_, wvpIndex);
}

void Renderer::DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection) {
	if (!line_) {
		Logger::Log("DrawGridBatch: Line is not initialized\n");
		return;
	}
	const uint32_t kGridWvpIndex = 0;
	line_->SetWvpMatrix(view * projection, kGridWvpIndex);
	line_->SetPipelineCommands(commandList_);
	line_->DrawBatch(commandList_, 0, 202, kGridWvpIndex);
}

void Renderer::DrawSprite(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform) {
	Matrix4x4 wvpMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation) * view * projection;
	sprite_->SetWvpMatrix(wvpMatrix);
	sprite_->SetPipelineCommands(commandList_, &textureManager_, TextureID::Texture3);
	sprite_->Draw(commandList_, 0);
}

void Renderer::DrawSphere(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, TextureID textureID) {
	Matrix4x4 wvpMatrix = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation) * view * projection;
	sphere_->SetWvpMatrix(wvpMatrix);
	sphere_->SetPipelineCommands(commandList_, &textureManager_, textureID);
	sphere_->Draw(commandList_, 0);
}
