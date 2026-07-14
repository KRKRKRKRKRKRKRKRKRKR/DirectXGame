#include "Renderer.h"
#include "../../Utils/Logger.h"
#include "../../../Math/MatrixMath.h"
#include "../DescriptorHeaps/DescriptorHeaps.h"
#include "../../../Externals/imgui/imgui.h"

void Renderer::Resize(int width, int height) {
	windowWidth_ = width;
	windowHeight_ = height;
	// GPUがDepthStencilバッファを使い終わっている前提（呼び出し元でGPU待機済み）
	textureManager_.InitializeDepthStencil(width, height, heaps_);

	// 鏡の反射先もメインウィンドウと同じ解像度で作り直す。TextureHandle/ヒープindexは維持されるため
	// mirrorRTVHandle_/mirrorDSVHandle_（固定index由来のCPUハンドル値）は再取得不要
	textureManager_.ResizeMirrorRenderTarget(width, height, heaps_);
	mirrorColorState_ = D3D12_RESOURCE_STATE_RENDER_TARGET; // 新しいリソースはRENDER_TARGET状態で生成されるため追跡をリセット
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

	// 鏡の反射描画先（RTV兼SRV）と専用深度バッファを用意する
	mirrorTextureHandle_ = textureManager_.CreateMirrorRenderTargetTexture(width, height, heaps);
	textureManager_.InitializeMirrorDepthStencil(width, height, heaps);
	mirrorRTVHandle_ = heaps->GetRTVHandle(2);
	mirrorDSVHandle_ = heaps->GetDSVHandle(1);

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
	// 鏡の反射パスと通常パスが同一フレーム内でインスタンス配列を衝突なく使い分けられるよう、
	// フレーム先頭でウォーターマークを0に戻す
	nextCubeInstanceOffset_     = 0;
	nextSphereInstanceOffset_   = 0;
	nextTriangleInstanceOffset_ = 0;
	nextModelInstanceOffset_    = 0;
	nextSprite3DInstanceOffset_ = 0;
	nextSprite2DInstanceOffset_ = 0;
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

TextureHandle Renderer::CreateTextureFromPixels(uint32_t width, uint32_t height, const uint8_t* rgbaPixels) {
	return textureManager_.CreateFromPixels(width, height, rgbaPixels, heaps_);
}

bool Renderer::UpdateTextureFromPixels(TextureHandle handle, uint32_t width, uint32_t height, const uint8_t* rgbaPixels) {
	return textureManager_.UpdatePixels(handle, width, height, rgbaPixels);
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

	// 鏡の反射パスと通常パスが同一フレーム内で同じModelインスタンス配列に書き込んでも
	// 衝突しないよう、ウォーターマークからの相対indexを使う
	uint32_t base = nextModelInstanceOffset_;
	for (int i = 0; i < (int)modelCommands_.size(); i++) {
		auto& cmd = modelCommands_[i];
		Model* model = models_[cmd.handle].get();
		uint32_t idx = base + static_cast<uint32_t>(i);

		model->SetWvpMatrix(cmd.wvp, cmd.world, idx);
		model->SetColor(cmd.color, idx);

		// 呼び出し側がテクスチャを指定していればそれを使い、なければモデルのMTLテクスチャを使う
		TextureHandle tex = (cmd.texture != kTextureNone) ? cmd.texture : model->GetTextureHandle();
		IssueDrawCommand(model, tex, cmd.blendMode, cmd.blendStrength, cmd.enableAlphaTest, cmd.alphaThreshold, cmd.useLighting, idx);
	}
	nextModelInstanceOffset_ = base + static_cast<uint32_t>(modelCommands_.size());

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
	uint32_t base = nextTriangleInstanceOffset_;
	uint32_t count = static_cast<uint32_t>(triangleCommands_.size());
	FlushBatch(triangleCommands_, triangle_.get(), base);
	nextTriangleInstanceOffset_ = base + count;
}

void Renderer::FlushCubes() {
	uint32_t base = nextCubeInstanceOffset_;
	uint32_t count = static_cast<uint32_t>(cubeCommands_.size());
	FlushBatch(cubeCommands_, cube_.get(), base);
	nextCubeInstanceOffset_ = base + count;
}

void Renderer::FlushSpheres() {
	uint32_t base = nextSphereInstanceOffset_;
	uint32_t count = static_cast<uint32_t>(sphereCommands_.size());
	FlushBatch(sphereCommands_, sphere_.get(), base);
	nextSphereInstanceOffset_ = base + count;
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
		line_->SetColor(lineCommands_[i].color, i); // インデックスを渡し、インスタンスごとに独立した色にする
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
	sprite3DCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold, std::nullopt, uvTransform });
}

void Renderer::DrawSprite2D(const Transform& transform, const Vector4& color, TextureHandle texture, bool useLighting, const UVTransform& uvTransform, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	// 正射影の範囲を実ウィンドウサイズではなく固定のデザイン解像度(kUiDesignWidth/Height)にすることで、
	// ビューポート自体は画面全体を覆ったまま、UI座標系だけがウィンドウの拡大縮小に対して自動的に
	// 引き伸ばされる（=translation/scaleのpx値を変えずにウィンドウ全体に対する相対サイズが保たれる）
	Matrix4x4 ortho  = MatrixMath::MakeOrthographicMatrix(kUiDesignWidth, kUiDesignHeight);
	Matrix4x4 world  = TransformMath::MakeAffineMatrix(transform.scale, transform.rotation, transform.translation);
	Matrix4x4 wvp    = world * ortho;
	sprite2DCommands_.push_back({ wvp, world, color, texture, useLighting, blendMode, blendStrength, enableAlphaTest, alphaThreshold, std::nullopt, uvTransform });
}

void Renderer::FlushSprites3D() {
	if (sprite3DCommands_.empty()) return;

	// Cube/Sphere等と同じインスタンス配列パターン：indexごとに独立したスロットへWVP/色を書き込む。
	// UVTransformは頂点バッファ（全インスタンス共有）に焼き込む都合上、コマンドごとに
	// SetUVTransform→即IssueDrawCommandする（バッチ化はしない。順序はwvpColorBuffer_書き込み後で問題ない）
	uint32_t base = nextSprite3DInstanceOffset_;
	for (int i = 0; i < (int)sprite3DCommands_.size(); i++) {
		auto& cmd = sprite3DCommands_[i];
		uint32_t idx = base + static_cast<uint32_t>(i);
		sprite3D_->SetWvpMatrix(cmd.wvp, cmd.world, idx);
		sprite3D_->SetColor(cmd.color, idx);
		sprite3D_->SetUVTransform(cmd.uvTransform);
		IssueDrawCommand(sprite3D_.get(), cmd.texture, cmd.blendMode, cmd.blendStrength, cmd.enableAlphaTest, cmd.alphaThreshold, cmd.useLighting, idx);
	}
	nextSprite3DInstanceOffset_ = base + static_cast<uint32_t>(sprite3DCommands_.size());

	sprite3DCommands_.clear();
}

void Renderer::FlushSprites2D() {
	if (sprite2DCommands_.empty()) return;

	uint32_t base = nextSprite2DInstanceOffset_;
	for (int i = 0; i < (int)sprite2DCommands_.size(); i++) {
		auto& cmd = sprite2DCommands_[i];
		uint32_t idx = base + static_cast<uint32_t>(i);
		sprite2D_->SetWvpMatrix(cmd.wvp, cmd.world, idx);
		sprite2D_->SetColor(cmd.color, idx);
		sprite2D_->SetUVTransform(cmd.uvTransform);
		IssueDrawCommand(sprite2D_.get(), cmd.texture, cmd.blendMode, cmd.blendStrength, cmd.enableAlphaTest, cmd.alphaThreshold, cmd.useLighting, idx);
	}
	nextSprite2DInstanceOffset_ = base + static_cast<uint32_t>(sprite2DCommands_.size());

	sprite2DCommands_.clear();
}

// ---- FlushAll / 鏡（反射）----

void Renderer::FlushAll() {
	FlushTriangles();
	FlushCubes();
	FlushModels();
	FlushLines();
	FlushSpheres();
	FlushSprites3D();
	FlushSprites2D(); // 2DスプライトはDepth無効で最後に描画 → 常に最前面
}

void Renderer::BeginMirrorPass(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& cameraPosition) {
	// 反射テクスチャがシェーダー参照可能状態（前フレームのPIXEL_SHADER_RESOURCE）なら、
	// 描画先として使えるようRENDER_TARGETへ戻す。初回フレームは生成時から既にRENDER_TARGETなので不要
	if (mirrorColorState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = textureManager_.GetMirrorColorResource();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = mirrorColorState_;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		commandList_->ResourceBarrier(1, &barrier);
		mirrorColorState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	commandList_->OMSetRenderTargets(1, &mirrorRTVHandle_, false, &mirrorDSVHandle_);
	Vector4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	commandList_->ClearRenderTargetView(mirrorRTVHandle_, reinterpret_cast<float*>(&clearColor), 0, nullptr);
	commandList_->ClearDepthStencilView(mirrorDSVHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(windowWidth_);
	viewport.Height = static_cast<float>(windowHeight_);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissorRect{ 0, 0, windowWidth_, windowHeight_ };
	commandList_->RSSetViewports(1, &viewport);
	commandList_->RSSetScissorRects(1, &scissorRect);

	SetCamera(view, projection, cameraPosition);
}

void Renderer::EndMirrorPass() {
	// ここまでに積まれた描画コマンドを反射用RTVへ実際に描画する
	FlushAll();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = textureManager_.GetMirrorColorResource();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = mirrorColorState_;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	commandList_->ResourceBarrier(1, &barrier);
	mirrorColorState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	// 以降の通常パス描画に備えて、RTV/DSV・ビューポートをバックバッファ側へ戻す
	// （BeginFrameで既にクリア済みのため、ここで再クリアはしない）
	commandList_->OMSetRenderTargets(1, &backBufferRTV_, false, &backBufferDSV_);
	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(windowWidth_);
	viewport.Height = static_cast<float>(windowHeight_);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	D3D12_RECT scissorRect{ 0, 0, windowWidth_, windowHeight_ };
	commandList_->RSSetViewports(1, &viewport);
	commandList_->RSSetScissorRects(1, &scissorRect);
}

void Renderer::DrawImGui() {
	ImGui::Begin("Mesh Settings");

	if (ImGui::SliderFloat("Cube Smoothness", &cubeSmoothness_, 0.0f, 1.0f)) {
		SetCubeSmoothness(cubeSmoothness_);
	}
	if (ImGui::SliderFloat("Triangle Smoothness", &triangleSmoothness_, 0.0f, 1.0f)) {
		SetTriangleSmoothness(triangleSmoothness_);
	}
	if (ImGui::SliderInt("Sphere Subdivision", &sphereSubdivision_, 1, static_cast<int>(kSphereMaxSubdivision))) {
		SetSphereSubdivision(static_cast<uint32_t>(sphereSubdivision_));
	}

	ImGui::End();
}
