#include "Renderer.h"
#include "../../Utils/Logger.h"
#include "../../../Math/MatrixMath.h"
#include "../DescriptorHeaps/DescriptorHeaps.h"

void Renderer::Resize(int width, int height) {
	windowWidth_ = width;
	windowHeight_ = height;
	// GPUがDepthStencilバッファを使い終わっている前提（呼び出し元でGPU待機済み）
	textureManager_.InitializeDepthStencil(width, height, heaps_);
}

void Renderer::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorHeaps* heaps, int width, int height) {
	device_ = device;
	commandList_ = commandList;
	windowWidth_ = width;
	windowHeight_ = height;

	shaderCompiler_.InitializeDXC();
	pipeline_.Initialize(device_, &shaderCompiler_);
	spritePipeline2D_.Initialize(device_, &shaderCompiler_, false); // depth無効（UI用）
	linePipeline_.Initialize(device_, &shaderCompiler_);
	skinnedPipeline_.Initialize(device_, &shaderCompiler_);

	heaps_ = heaps;
	textureManager_.SetDevice(device_);
	textureManager_.SetCommandList(commandList_);
	textureManager_.InitializeDepthStencil(width, height, heaps);
	textureManager_.InitializeDefaultTexture(heaps);

	triangle_ = std::make_unique<Triangle>();
	triangle_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &pipeline_, heaps);

	cube_ = std::make_unique<Cube>();
	cube_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &pipeline_, heaps);

	line_ = std::make_unique<Line>();
	line_->Initialize(device_, &textureManager_, linePipeline_.GetRootSignature(), linePipeline_.GetPipelineState());

	sprite3D_ = std::make_unique<Sprite>();
	uint32_t sprite3DSrvBase = heaps_->AllocateSRVIndex(2);
	sprite3D_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &pipeline_, heaps_, sprite3DSrvBase, sprite3DSrvBase + 1);

	sprite2D_ = std::make_unique<Sprite>();
	uint32_t sprite2DSrvBase = heaps_->AllocateSRVIndex(2);
	sprite2D_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &spritePipeline2D_, heaps_, sprite2DSrvBase, sprite2DSrvBase + 1);
	sprite2D_->SetFlipV(true); // スクリーン座標系（Y-down）に合わせて V を反転

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &pipeline_, heaps);

	light_.Initialize(device_);

	InitializeGridLines();
	triangleCommands_.reserve(4096);
	cubeCommands_.reserve(Cube::kMaxInstanceCount);
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

Renderer::ModelHandle Renderer::LoadModel(const std::string& directoryPath, const std::string& filename) {
	auto model = std::make_unique<Model>();
	// wvp, 色, ボーン行列パレット用に3枠払い出してもらう
	uint32_t srvBase = heaps_->AllocateSRVIndex(3);
	model->Initialize(device_, &textureManager_,
		pipeline_.GetRootSignature(), &pipeline_,
		&skinnedPipeline_, skinnedPipeline_.GetRootSignature(),
		heaps_,
		directoryPath, filename,
		srvBase, srvBase + 1, srvBase + 2);

	ModelHandle handle = static_cast<ModelHandle>(models_.size());
	models_.push_back(std::move(model));
	return handle;
}

void Renderer::UpdateModelAnimation(ModelHandle handle, float deltaTime) {
	models_[handle]->UpdateAnimation(deltaTime);
}

void Renderer::DrawModel(ModelHandle handle, const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	modelCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold, std::nullopt, handle });
}

void Renderer::FlushModels() {
	if (modelCommands_.empty()) return;

	for (int i = 0; i < (int)modelCommands_.size(); i++) {
		auto& cmd = modelCommands_[i];
		Model* model = models_[cmd.handle].get();

		model->SetWvpMatrix(cmd.wvp, cmd.world, i);
		model->SetColor(cmd.color, i);

		// 呼び出し側がテクスチャを指定していればそれを使い、なければモデルのMTLテクスチャを使う
		TextureHandle tex = (cmd.texture != kTextureNone) ? cmd.texture : model->GetTextureHandle();
		IssueDrawCommand(model, tex, cmd.blendMode, cmd.blendStrength, cmd.enableAlphaTest, cmd.alphaThreshold, cmd.useLighting, static_cast<uint32_t>(i));
	}

	modelCommands_.clear();
}

// ---- Transform 版 ----

void Renderer::DrawTriangle(const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	triangleCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold });
}

void Renderer::DrawSphere(const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	sphereCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold });
}

void Renderer::DrawCube(const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	cubeCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold });
}

void Renderer::DrawCube(const Transform& t, const Matrix4x4& worldInverseTranspose, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	cubeCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold, worldInverseTranspose });
}

// ---- WVP 直接指定版 ----

void Renderer::DrawTriangle(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	triangleCommands_.push_back({ wvp, MatrixMath::Identity(), color, texture, useLighting, blendMode, blendStrength, false, 0.5f });
}

void Renderer::DrawCube(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	cubeCommands_.push_back({ wvp, MatrixMath::Identity(), color, texture, useLighting, blendMode, blendStrength, false, 0.5f });
}

void Renderer::DrawSphere(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	sphereCommands_.push_back({ wvp, MatrixMath::Identity(), color, texture, useLighting, blendMode, blendStrength, false, 0.5f });
}

// ---- Flush ----

void Renderer::FlushTriangles() {
	FlushBatch(triangleCommands_, triangle_.get());
}

void Renderer::FlushCubes() {
	FlushBatch(cubeCommands_, cube_.get());
}

void Renderer::FlushSpheres() {
	FlushBatch(sphereCommands_, sphere_.get());
}

// ---- Line / Grid ----

void Renderer::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, const Matrix4x4& view, const Matrix4x4& projection) {
	lineCommands_.push_back({ start, end, color, view * projection });
}

void Renderer::FlushLines() {
	if (lineCommands_.empty()) return;

	line_->SetPipelineCommands(commandList_);

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

// ---- Sprite ----

void Renderer::DrawSprite3D(const Transform& transform, const Vector4& color, TextureHandle texture, bool useLighting, const UVTransform& uvTransform, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	sprite3D_->SetWvpMatrix(wvp, world);
	sprite3D_->SetColor(color);
	sprite3D_->SetUVTransform(uvTransform);
	IssueDrawCommand(sprite3D_.get(), texture, blendMode, blendStrength, enableAlphaTest, alphaThreshold, useLighting, 0u);
}

void Renderer::DrawSprite2D(const Transform& transform, const Vector4& color, TextureHandle texture, bool useLighting, const UVTransform& uvTransform, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	Matrix4x4 ortho  = MatrixMath::MakeOrthographicMatrix(
		static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
	Matrix4x4 world  = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	Matrix4x4 wvp    = world * ortho;
	sprite2DCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold, std::nullopt, uvTransform });
}

void Renderer::FlushSprites2D() {
	if (sprite2DCommands_.empty()) return;

	for (auto& cmd : sprite2DCommands_) {
		sprite2D_->SetWvpMatrix(cmd.wvp, cmd.world);
		sprite2D_->SetColor(cmd.color);
		sprite2D_->SetUVTransform(cmd.uvTransform);
		IssueDrawCommand(sprite2D_.get(), cmd.texture, cmd.blendMode, cmd.blendStrength, cmd.enableAlphaTest, cmd.alphaThreshold, cmd.useLighting, 0u);
	}

	sprite2DCommands_.clear();
}
