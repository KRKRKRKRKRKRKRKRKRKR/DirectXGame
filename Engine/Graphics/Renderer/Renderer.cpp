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
	sprite3D_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &pipeline_, heaps_, 16, 17);

	sprite2D_ = std::make_unique<Sprite>();
	sprite2D_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &spritePipeline2D_, heaps_, 18, 19);
	sprite2D_->SetFlipV(true); // スクリーン座標系（Y-down）に合わせて V を反転

	sphere_ = std::make_unique<Sphere>();
	sphere_->Initialize(device_, &textureManager_, pipeline_.GetRootSignature(), &pipeline_, heaps);

	light_.Initialize(device_);

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

Renderer::ModelHandle Renderer::LoadModel(const std::string& directoryPath, const std::string& filename) {
	auto model = std::make_unique<Model>();
	model->Initialize(device_, &textureManager_,
		pipeline_.GetRootSignature(), &pipeline_, heaps_,
		directoryPath, filename,
		nextModelHeapIndex_, nextModelHeapIndex_ + 1);
	nextModelHeapIndex_ += 2;

	ModelHandle handle = static_cast<ModelHandle>(models_.size());
	models_.push_back(std::move(model));
	return handle;
}

void Renderer::DrawModel(ModelHandle handle, const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	modelCommands_.push_back({ handle, wvp, world, color, texture, useLighting, blendMode, blendStrength });
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
		model->SetPipelineCommands(commandList_, &textureManager_, tex, cmd.blendMode, cmd.blendStrength);
		commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(cmd.useLighting));
		commandList_->SetGraphicsRoot32BitConstant(4, static_cast<UINT>(i), 0);
		model->Draw(commandList_, 1, i);
	}

	modelCommands_.clear();
}

// ---- Transform 版 ----

void Renderer::DrawTriangle(const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	triangleCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength });
}

void Renderer::DrawSphere(const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	sphereCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength });
}

void Renderer::DrawCube(const Transform& t, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	cubeCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength });
}

// ---- WVP 直接指定版 ----

void Renderer::DrawTriangle(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	triangleCommands_.push_back({ wvp, MatrixMath::Identity(), color, texture, useLighting, blendMode, blendStrength });
}

void Renderer::DrawCube(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	cubeCommands_.push_back({ wvp, MatrixMath::Identity(), color, texture, useLighting, blendMode, blendStrength });
}

void Renderer::DrawSphere(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture, bool useLighting, BlendMode blendMode, float blendStrength) {
	sphereCommands_.push_back({ wvp, MatrixMath::Identity(), color, texture, useLighting, blendMode, blendStrength });
}

// ---- Flush ----

void Renderer::FlushTriangles() {
	if (triangleCommands_.empty()) return;

	std::stable_sort(triangleCommands_.begin(), triangleCommands_.end(),
		[](const TriangleCommand& a, const TriangleCommand& b) {
			if (a.texture != b.texture) return a.texture < b.texture;
			if (a.blendMode != b.blendMode) return a.blendMode < b.blendMode;
			if (a.blendStrength != b.blendStrength) return a.blendStrength < b.blendStrength;
			return (int)a.useLighting > (int)b.useLighting;
		});

	for (int i = 0; i < (int)triangleCommands_.size(); i++) {
		triangle_->SetWvpMatrix(triangleCommands_[i].wvp, triangleCommands_[i].world, i);
		triangle_->SetColor(triangleCommands_[i].color, i);
	}

	int start = 0;
	while (start < (int)triangleCommands_.size()) {
		TextureHandle currentTex    = triangleCommands_[start].texture;
		bool          batchLighting = triangleCommands_[start].useLighting;
		BlendMode     batchBlend    = triangleCommands_[start].blendMode;
		float         batchStrength = triangleCommands_[start].blendStrength;
		int end = start;
		while (end < (int)triangleCommands_.size() &&
			   triangleCommands_[end].texture       == currentTex &&
			   triangleCommands_[end].useLighting   == batchLighting &&
			   triangleCommands_[end].blendMode     == batchBlend &&
			   triangleCommands_[end].blendStrength == batchStrength) end++;
		triangle_->SetPipelineCommands(commandList_, &textureManager_, currentTex, batchBlend, batchStrength);
		commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(batchLighting));
		commandList_->SetGraphicsRoot32BitConstant(4, (UINT)start, 0);
		triangle_->Draw(commandList_, end - start, start);
		start = end;
	}

	triangleCommands_.clear();
}

void Renderer::FlushCubes() {
	if (cubeCommands_.empty()) return;

	std::stable_sort(cubeCommands_.begin(), cubeCommands_.end(),
		[](const CubeCommand& a, const CubeCommand& b) {
			if (a.texture != b.texture) return a.texture < b.texture;
			if (a.blendMode != b.blendMode) return a.blendMode < b.blendMode;
			if (a.blendStrength != b.blendStrength) return a.blendStrength < b.blendStrength;
			return (int)a.useLighting > (int)b.useLighting;
		});

	for (int i = 0; i < (int)cubeCommands_.size(); i++) {
		cube_->SetWvpMatrix(cubeCommands_[i].wvp, cubeCommands_[i].world, i);
		cube_->SetColor(cubeCommands_[i].color, i);
	}

	int start = 0;
	while (start < (int)cubeCommands_.size()) {
		TextureHandle currentTex    = cubeCommands_[start].texture;
		bool          batchLighting = cubeCommands_[start].useLighting;
		BlendMode     batchBlend    = cubeCommands_[start].blendMode;
		float         batchStrength = cubeCommands_[start].blendStrength;
		int end = start;
		while (end < (int)cubeCommands_.size() &&
			   cubeCommands_[end].texture       == currentTex &&
			   cubeCommands_[end].useLighting   == batchLighting &&
			   cubeCommands_[end].blendMode     == batchBlend &&
			   cubeCommands_[end].blendStrength == batchStrength) end++;
		cube_->SetPipelineCommands(commandList_, &textureManager_, currentTex, batchBlend, batchStrength);
		commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(batchLighting));
		commandList_->SetGraphicsRoot32BitConstant(4, (UINT)start, 0);
		cube_->Draw(commandList_, end - start, start);
		start = end;
	}

	cubeCommands_.clear();
}

void Renderer::FlushSpheres() {
	if (sphereCommands_.empty()) return;

	std::stable_sort(sphereCommands_.begin(), sphereCommands_.end(),
		[](const SphereCommand& a, const SphereCommand& b) {
			if (a.texture != b.texture) return a.texture < b.texture;
			if (a.blendMode != b.blendMode) return a.blendMode < b.blendMode;
			if (a.blendStrength != b.blendStrength) return a.blendStrength < b.blendStrength;
			return (int)a.useLighting > (int)b.useLighting;
		});

	for (int i = 0; i < (int)sphereCommands_.size(); i++) {
		sphere_->SetWvpMatrix(sphereCommands_[i].wvp, sphereCommands_[i].world, i);
		sphere_->SetColor(sphereCommands_[i].color, i);
	}

	int start = 0;
	while (start < (int)sphereCommands_.size()) {
		TextureHandle currentTex    = sphereCommands_[start].texture;
		bool          batchLighting = sphereCommands_[start].useLighting;
		BlendMode     batchBlend    = sphereCommands_[start].blendMode;
		float         batchStrength = sphereCommands_[start].blendStrength;
		int end = start;
		while (end < (int)sphereCommands_.size() &&
			   sphereCommands_[end].texture       == currentTex &&
			   sphereCommands_[end].useLighting   == batchLighting &&
			   sphereCommands_[end].blendMode     == batchBlend &&
			   sphereCommands_[end].blendStrength == batchStrength) end++;
		sphere_->SetPipelineCommands(commandList_, &textureManager_, currentTex, batchBlend, batchStrength);
		commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(batchLighting));
		commandList_->SetGraphicsRoot32BitConstant(4, (UINT)start, 0);
		sphere_->Draw(commandList_, end - start, start);
		start = end;
	}

	sphereCommands_.clear();
}

// ---- Line / Grid ----

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

// ---- Sprite ----

void Renderer::DrawSprite3D(const Transform& transform, const Vector4& color, TextureHandle texture, bool useLighting, const UVTransform& uvTransform, BlendMode blendMode, float blendStrength) {
	Matrix4x4 world = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	Matrix4x4 wvp   = world * view_ * projection_;
	sprite3D_->SetWvpMatrix(wvp, world);
	sprite3D_->SetColor(color);
	sprite3D_->SetUVTransform(uvTransform);
	sprite3D_->SetPipelineCommands(commandList_, &textureManager_, texture, blendMode, blendStrength);
	commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(useLighting));
	commandList_->SetGraphicsRoot32BitConstant(4, 0u, 0);
	sprite3D_->Draw(commandList_, 0);
}

void Renderer::DrawSprite2D(const Transform& transform, const Vector4& color, TextureHandle texture, bool useLighting, const UVTransform& uvTransform, BlendMode blendMode, float blendStrength) {
	Matrix4x4 ortho  = MatrixMath::MakeOrthographicMatrix(
		static_cast<float>(windowWidth_), static_cast<float>(windowHeight_));
	Matrix4x4 world  = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	Matrix4x4 wvp    = world * ortho;
	sprite2DCommands_.push_back({ wvp, world, color, texture, useLighting, uvTransform, blendMode, blendStrength });
}

void Renderer::FlushSprites2D() {
	if (sprite2DCommands_.empty()) return;

	for (auto& cmd : sprite2DCommands_) {
		sprite2D_->SetWvpMatrix(cmd.wvp, cmd.world);
		sprite2D_->SetColor(cmd.color);
		sprite2D_->SetUVTransform(cmd.uvTransform);
		sprite2D_->SetPipelineCommands(commandList_, &textureManager_, cmd.texture, cmd.blendMode, cmd.blendStrength);
		commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(cmd.useLighting));
		commandList_->SetGraphicsRoot32BitConstant(4, 0u, 0);
		sprite2D_->Draw(commandList_, 0);
	}

	sprite2DCommands_.clear();
}
