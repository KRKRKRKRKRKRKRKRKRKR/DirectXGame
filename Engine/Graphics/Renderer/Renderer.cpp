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

	heaps_ = heaps;
	textureManager_.SetDevice(device_);
	textureManager_.SetCommandList(commandList_);
	textureManager_.InitializeDepthStencil(width, height, heaps);
	textureManager_.InitializeDefaultTexture(heaps);  // handle=0 を白テクスチャとして登録

	triangle_ = std::make_unique<Triangle>();
	triangle_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState(), heaps);

	cube_ = std::make_unique<Cube>();
	cube_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState(), heaps);

	line_ = std::make_unique<Line>();
	line_->Initialize(device_, &textureManager_, linePipeline_.GetRootSignature(), linePipeline_.GetPipelineState());

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), pipeline_.GetPipelineState());

	InitializeGridLines();
	triangleCommands_.reserve(4096);
	cubeCommands_.reserve(4096);
	lineCommands_.reserve(1024);
	sphereCommands_.reserve(64);

	Logger::Log("Complete Initialize Renderer\n");
}

void Renderer::Finalize() {
	textureManager_.Finalize();
}

void Renderer::ResetFrameIndex() {
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

TextureHandle Renderer::LoadTexture(const std::string& filePath) {
	return textureManager_.Load(filePath, heaps_);
}

void Renderer::DrawTriangle(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture) {
	triangleCommands_.push_back({ wvp, color, texture });
}

void Renderer::FlushTriangles() {
	if (triangleCommands_.empty()) return;

	std::stable_sort(triangleCommands_.begin(), triangleCommands_.end(),
		[](const TriangleCommand& a, const TriangleCommand& b) {
			return a.texture < b.texture;
		});

	for (int i = 0; i < (int)triangleCommands_.size(); i++) {
		triangle_->SetWvpMatrix(triangleCommands_[i].wvp, i);
		triangle_->SetColor(triangleCommands_[i].color, i);
	}

	int start = 0;
	while (start < (int)triangleCommands_.size()) {
		TextureHandle currentTex = triangleCommands_[start].texture;
		int end = start;
		while (end < (int)triangleCommands_.size() && triangleCommands_[end].texture == currentTex) end++;
		triangle_->SetPipelineCommands(commandList_, &textureManager_, currentTex);
		triangle_->Draw(commandList_, end - start, start);
		start = end;
	}
	
	triangleCommands_.clear();
}

void Renderer::DrawCube(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture) {
	cubeCommands_.push_back({ wvp, color, texture });
}

void Renderer::FlushCubes() {
	if (cubeCommands_.empty()) return;

	std::stable_sort(cubeCommands_.begin(), cubeCommands_.end(),
		[](const CubeCommand& a, const CubeCommand& b) {
			return a.texture < b.texture;
		});

	for (int i = 0; i < (int)cubeCommands_.size(); i++) {
		cube_->SetWvpMatrix(cubeCommands_[i].wvp, i);
		cube_->SetColor(cubeCommands_[i].color, i);
	}

	int start = 0;
	while (start < (int)cubeCommands_.size()) {
		TextureHandle currentTex = cubeCommands_[start].texture;
		int end = start;
		while (end < (int)cubeCommands_.size() && cubeCommands_[end].texture == currentTex) end++;
		cube_->SetPipelineCommands(commandList_, &textureManager_, currentTex);
		cube_->Draw(commandList_, end - start, start);
		start = end;
	}

	cubeCommands_.clear();
}

void Renderer::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, const Matrix4x4& view, const Matrix4x4& projection) {
	lineCommands_.push_back({ start, end, color, view * projection });
}

void Renderer::FlushLines() {
	if (lineCommands_.empty()) return;

	line_->SetPipelineCommands(commandList_);
	line_->SetColor(lineCommands_[0].color);

	for (int i = 0; i < (int)lineCommands_.size(); i++) {
		line_->SetLine(lineCommands_[i].start, lineCommands_[i].end, i);
		line_->SetWvpMatrix(lineCommands_[i].viewProj, i);
		line_->SetColor(lineCommands_[i].color);
		line_->Draw(commandList_, 1, i);
	}

	lineCommands_.clear();
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
	sprite_->SetPipelineCommands(commandList_, &textureManager_, kTextureNone);
	sprite_->Draw(commandList_, 0);
}

void Renderer::DrawSphere(const Matrix4x4& wvp, TextureHandle texture) {
	sphereCommands_.push_back({ wvp, texture });
}

void Renderer::FlushSpheres() {
	if (sphereCommands_.empty()) return;

	for (const auto& cmd : sphereCommands_) {
		sphere_->SetWvpMatrix(cmd.wvp);
		sphere_->SetPipelineCommands(commandList_, &textureManager_, cmd.texture);
		sphere_->Draw(commandList_, 1, 0);
	}

	sphereCommands_.clear();
}
