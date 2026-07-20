#pragma once

#include <d3d12.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <string>
#include <optional>
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../Pipeline/Pipeline.h"
#include "../Pipeline/LinePipeline.h"
#include "../Pipeline/SkinnedPipeline.h"
#include "../Texture/TextureManager.h"
#include "../Object/Triangle/Triangle.h"
#include "../Object/Cube/Cube.h"
#include "../Object/Line/Line.h"
#include "../Object/Sprite/Sprite.h"
#include "../Object/Sphere/Sphere.h"
#include "../Object/Model/Model.h"
#include "../Lighting/SceneLight.h"
#include "../../../Math/MathTypes.h"
#include "../../../Math/TransformMath.h"

class DescriptorHeaps;

class Renderer {
public:
	Renderer() = default;
	~Renderer() = default;

	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorHeaps* heaps, int width, int height);
	void Finalize();

	// ウィンドウリサイズ時に呼ぶ。DepthStencilバッファを新しいサイズで再生成する
	void Resize(int width, int height);

	void SetCommandList(ID3D12GraphicsCommandList* commandList) { commandList_ = commandList; }

	// テクスチャをファイルから読み込んでハンドルを返す（同じパスは二重ロードしない）
	TextureHandle LoadTexture(const std::string& filePath);

	// メモリ上のRGBA8ピクセル列からテクスチャを作成してハンドルを返す（フォント合成ビットマップ等）
	TextureHandle CreateTextureFromPixels(uint32_t width, uint32_t height, const uint8_t* rgbaPixels);

	// ImGui::Image()等、ImTextureIDとして直接キャストして使うためのGPUディスクリプタハンドルを返す
	// （プロジェクトパネルの画像サムネイル表示用）
	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureSrvGpuHandle(TextureHandle handle) const { return textureManager_.GetSrvGpuHandle(handle); }

	// 既存テクスチャの中身だけを同サイズのピクセルで置き換える（HUD等、毎フレーム更新する用途）
	bool UpdateTextureFromPixels(TextureHandle handle, uint32_t width, uint32_t height, const uint8_t* rgbaPixels);

	// OBJ を読み込んでハンドルを返す
	using ModelHandle = uint32_t;
	ModelHandle LoadModel(const std::string& directoryPath, const std::string& filename);

	void DrawModel(ModelHandle handle, const Transform& t, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);
	void FlushModels();

	// ボーン付きModelのアニメーションを進める。毎フレームGame::Update等から呼ぶ
	void UpdateModelAnimation(ModelHandle handle, float deltaTime);

	// フレーム先頭で一度だけ呼ぶ。以降の Draw(Transform,...) はこの行列を使う
	// cameraPosition はスペキュラ/リムライト計算用に毎フレームライトへ転送する
	void SetCamera(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& cameraPosition) {
		view_ = view; projection_ = projection;
		light_.SetCameraPosition(cameraPosition);
	}

	// Transform 版（内部で world * view * proj を計算）
	void DrawTriangle(const Transform& t, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);
	void DrawSphere  (const Transform& t, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);
	void DrawCube    (const Transform& t, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);

	// worldInverseTransposeを呼び出し側で計算済みの場合はこちらを使う（Inverse+Transpose計算を省略できる）。
	// 大量の静止インスタンス（グリッド等）で回転が変化した時だけ再計算してキャッシュする用途向け
	void DrawCube    (const Transform& t, const Matrix4x4& worldInverseTranspose, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);

	// WVP 直接指定版（既存、後方互換用）
	void DrawTriangle(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f);
	void DrawSphere  (const Matrix4x4& wvp, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f);
	void DrawCube    (const Matrix4x4& wvp, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f);

	void FlushTriangles();
	void FlushSpheres();
	void FlushCubes();

	// フレーム先頭（BeginFrame直後）に呼ぶ。バックバッファのRTV/DSVをキャッシュし、
	// 鏡の反射パス終了後にRTV/DSVバインドを戻す先として使う
	void SetBackBufferTarget(D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv) {
		backBufferRTV_ = rtv;
		backBufferDSV_ = dsv;
	}

	// 鏡の反射パス：RTV/DSVを反射用オフスクリーンに切り替えてクリアし、反射視点のカメラを設定する。
	// この後に通常のDrawXxxで反射に映すオブジェクトを積み、EndMirrorPass()で確定させる
	void BeginMirrorPass(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& cameraPosition);
	// 積まれた描画コマンドを反射用RTVへFlushし、シェーダーから読めるようバリアを張り、
	// RTV/DSVバインドをバックバッファへ戻す
	void EndMirrorPass();
	// 鏡面オブジェクトに貼るためのテクスチャハンドル（反射結果）
	TextureHandle GetMirrorTextureHandle() const { return mirrorTextureHandle_; }

	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, const Matrix4x4& view, const Matrix4x4& projection);
	void FlushLines();
	void DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection);
	// 3Dスプライト（カメラ付き WVP）。FlushSprites3D() で実際に描画される
	void DrawSprite3D(const Transform& transform, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true, const UVTransform& uvTransform = {}, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);
	void FlushSprites3D();
	// 2DスプライトUI（ピクセル座標、奥行きなし）。FlushSprites2D() で最後に描画される
	void DrawSprite2D(const Transform& transform, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = false, const UVTransform& uvTransform = {}, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);
	void FlushSprites2D();

	// Triangles→Cubes→Models→Lines→Spheres→Sprites2Dの順で全部Flushする。通常の
	// Engine::Flush()と鏡のEndMirrorPass()の両方から使う共通処理
	void FlushAll();

	SceneLight& GetLight() { return light_; }

	void SetTriangleSmoothness(float s) { triangle_->SetSmoothness(s); }
	void SetCubeSmoothness(float s)     { cube_->SetSmoothness(s); }
	void SetSphereSubdivision(uint32_t subdivision) { sphere_->SetSubdivision(subdivision); }
	static constexpr uint32_t kSphereMaxSubdivision = Sphere::kMaxSubdivision;

	// Cube/Triangle SmoothnessとSphere Subdivisionは個体ごとのパラメータではなく、
	// cube_/triangle_/sphere_という単一の共有メッシュ全体に効くグローバル設定のため、
	// データを持つRenderer自身がImGuiを描画する（SceneLight::DrawImGui()と同じパターン）
	void DrawImGui();

	void InitializeGridLines();
	void ResetFrameIndex();

	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_; }
	int GetClientWidth() const { return windowWidth_; }
	int GetClientHeight() const { return windowHeight_; }

	// ドック中央ノード（Sceneビュー）の実際の画面矩形をD3D12のビューポート/シザーへ適用する。
	// Engine::Update()がImGuiManager::BeginFrame()の直後、毎フレーム1回呼ぶ。3D描画がウィンドウ
	// 全体ではなく、Hierarchy/Inspector/Project等のパネルに隠れない中央領域だけに収まるようにする
	void SetSceneViewportRect(float x, float y, float width, float height);

	// SetSceneViewportRectで設定した最新の矩形。アスペクト比計算（SceneBase::Render）や
	// マウス座標変換（GizmoController）がGetClientWidth/Heightの代わりにこちらを使う
	int GetSceneViewportWidth() const { return static_cast<int>(sceneViewportWidth_); }
	int GetSceneViewportHeight() const { return static_cast<int>(sceneViewportHeight_); }
	float GetSceneViewportOffsetX() const { return sceneViewportOffsetX_; }
	float GetSceneViewportOffsetY() const { return sceneViewportOffsetY_; }

	// DrawSprite2Dが使う固定デザイン解像度。GizmoController等、Sprite2Dの見た目・ピッキングを
	// 一致させたい呼び出し元はGetClientWidth/Heightではなくこちらを使う
	static float GetUiDesignWidth()  { return kUiDesignWidth; }
	static float GetUiDesignHeight() { return kUiDesignHeight; }

private:
	// Triangle/Cube/Sphereの3種で共通のコマンド情報。ソートキー・バッチ判定・
	// SetPipelineCommands呼び出しに使うフィールドをまとめて持つ
	struct DrawCommandBase {
		Matrix4x4     wvp;
		Matrix4x4     world;
		Vector4       color;
		TextureHandle texture;
		bool          useLighting;
		BlendMode     blendMode;
		float         blendStrength;
		bool          enableAlphaTest;
		float         alphaThreshold;
		// 未設定ならFlushBatch内でInverse+Transposeを計算する。呼び出し側で計算済みなら
		// ここに値を入れておくことで毎フレームの逆行列計算を省略できる（グリッド等の大量静止インスタンス向け）
		std::optional<Matrix4x4> worldInverseTranspose;
	};
	struct TriangleCommand : DrawCommandBase {};
	struct SphereCommand   : DrawCommandBase {};
	struct CubeCommand     : DrawCommandBase {};

	struct LineCommand {
		Vector3   start;
		Vector3   end;
		Vector4   color;
		Matrix4x4 viewProj;
	};
	struct Sprite2DCommand : DrawCommandBase {
		UVTransform uvTransform;
	};
	struct Sprite3DCommand : DrawCommandBase {
		UVTransform uvTransform;
	};
	struct ModelCommand : DrawCommandBase {
		ModelHandle handle;
	};

	// Triangle/Cube/Sphereの3種で共通のFlush処理（ソート→バッチ化→描画）。
	// objはSetWvpMatrix/SetColor/SetPipelineCommands/Drawという同一シグネチャの
	// メンバ関数を持つ型（Triangle*/Cube*/Sphere*）を渡す。instanceBaseは書き込み先の
	// インスタンス配列の開始位置（鏡の反射パスと通常パスが同一フレーム内で衝突しないための
	// ウォーターマーク、Renderer::ResetFrameIndex()で0にリセットされる）
	template<typename ObjectT, typename CommandT>
	void FlushBatch(std::vector<CommandT>& commands, ObjectT* obj, uint32_t instanceBase);

	// Model/Sprite3D/Sprite2Dで共通の「1件描画」シーケンス
	// （SetPipelineCommands→ライトCBV→インスタンスオフセット定数→Draw）。
	// バッチ化はせず、呼び出し側でWVP/色/UVTransform等を個別に設定済みの前提
	template<typename ObjectT>
	void IssueDrawCommand(ObjectT* obj, TextureHandle texture, BlendMode blendMode, float blendStrength,
		bool enableAlphaTest, float alphaThreshold, bool useLighting, uint32_t instanceOffset);

	ID3D12Device* device_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;

	ShaderCompiler shaderCompiler_;
	Pipeline pipeline_;
	Pipeline spritePipeline2D_;
	LinePipeline linePipeline_;
	SkinnedPipeline skinnedPipeline_; // ボーン付きModel専用
	TextureManager textureManager_;

	std::unique_ptr<Triangle> triangle_;
	std::unique_ptr<Cube>     cube_;
	std::unique_ptr<Line>     line_;
	std::unique_ptr<Sprite>   sprite3D_;
	std::unique_ptr<Sprite>   sprite2D_;
	std::unique_ptr<Sphere>   sphere_;
	SceneLight light_;

	std::vector<std::unique_ptr<Model>> models_;

	std::vector<TriangleCommand>  triangleCommands_;
	std::vector<CubeCommand>      cubeCommands_;
	std::vector<LineCommand>      lineCommands_;
	std::vector<SphereCommand>    sphereCommands_;
	std::vector<Sprite3DCommand>  sprite3DCommands_;
	std::vector<Sprite2DCommand>  sprite2DCommands_;
	std::vector<ModelCommand>     modelCommands_;

	DescriptorHeaps* heaps_ = nullptr;
	uint32_t currentLineIndex_ = 0;
	int windowWidth_ = 0;
	int windowHeight_ = 0;

	// SetSceneViewportRectで設定される、ドック中央ノード（Sceneビュー）の実際の画面矩形。
	// 呼ばれるまでの保険としてInitialize()でフルウィンドウ値を入れておく
	float sceneViewportOffsetX_ = 0.0f;
	float sceneViewportOffsetY_ = 0.0f;
	float sceneViewportWidth_ = 0.0f;
	float sceneViewportHeight_ = 0.0f;

	// Sprite2D（TextRenderComponent/SpriteRenderComponentの2D側）の座標・サイズはこの解像度を
	// 基準にデザインされている（main.cppの起動時ウィンドウサイズと一致）。DrawSprite2Dの正射影を
	// 常にこの固定サイズにすることで、実ウィンドウが拡大/縮小されてもUI全体が同じ相対サイズ・
	// 相対位置のまま拡縮される（ウィンドウの実ピクセル数に合わせてUIのpx値も変えずに済む）
	static constexpr float kUiDesignWidth  = 1280.0f;
	static constexpr float kUiDesignHeight = 720.0f;

	Matrix4x4 view_{};
	Matrix4x4 projection_{};

	// DrawImGui()のスライダー用の現在値キャッシュ
	float triangleSmoothness_ = 1.0f;
	float cubeSmoothness_     = 1.0f;
	int   sphereSubdivision_  = static_cast<int>(kSphereMaxSubdivision);

	// ---- 鏡（反射）関連 ----
	TextureHandle mirrorTextureHandle_ = kTextureNone;
	D3D12_CPU_DESCRIPTOR_HANDLE mirrorRTVHandle_{};
	D3D12_CPU_DESCRIPTOR_HANDLE mirrorDSVHandle_{};
	// 反射用カラーテクスチャの現在のリソース状態を追跡する（RENDER_TARGET/PIXEL_SHADER_RESOURCEを
	// 毎フレーム往復するため、初回フレームで不整合なバリアを発行しないように必要）
	D3D12_RESOURCE_STATES mirrorColorState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV_{};
	D3D12_CPU_DESCRIPTOR_HANDLE backBufferDSV_{};

	// インスタンスバッファのウォーターマーク（フレーム先頭でResetFrameIndex()により0にリセット）。
	// 反射パス・通常パスが同一フレーム内で同じCube/Sphere/Triangle/Modelインスタンス配列に
	// 書き込む際、互いの書き込み範囲が重ならないようにするための「次に書き込む位置」
	uint32_t nextCubeInstanceOffset_     = 0;
	uint32_t nextSphereInstanceOffset_   = 0;
	uint32_t nextTriangleInstanceOffset_ = 0;
	uint32_t nextModelInstanceOffset_    = 0;
	uint32_t nextSprite3DInstanceOffset_ = 0;
	uint32_t nextSprite2DInstanceOffset_ = 0;
};

// Triangle/Cube/Sphereで共通のFlush処理。ソートキー・バッチ判定はDrawCommandBaseの
// フィールド（texture→blendMode→blendStrength→enableAlphaTest→alphaThreshold→useLighting）で行う
template<typename ObjectT, typename CommandT>
void Renderer::FlushBatch(std::vector<CommandT>& commands, ObjectT* obj, uint32_t instanceBase) {
	if (commands.empty()) return;

	std::stable_sort(commands.begin(), commands.end(),
		[](const CommandT& a, const CommandT& b) {
			if (a.texture != b.texture) return a.texture < b.texture;
			if (a.blendMode != b.blendMode) return a.blendMode < b.blendMode;
			if (a.blendStrength != b.blendStrength) return a.blendStrength < b.blendStrength;
			if (a.enableAlphaTest != b.enableAlphaTest) return a.enableAlphaTest < b.enableAlphaTest;
			if (a.alphaThreshold != b.alphaThreshold) return a.alphaThreshold < b.alphaThreshold;
			return (int)a.useLighting > (int)b.useLighting;
		});

	for (int i = 0; i < (int)commands.size(); i++) {
		uint32_t idx = instanceBase + static_cast<uint32_t>(i);
		if (commands[i].worldInverseTranspose.has_value()) {
			obj->SetWvpMatrix(commands[i].wvp, commands[i].world, *commands[i].worldInverseTranspose, idx);
		} else {
			obj->SetWvpMatrix(commands[i].wvp, commands[i].world, idx);
		}
		obj->SetColor(commands[i].color, idx);
	}

	int start = 0;
	while (start < (int)commands.size()) {
		TextureHandle currentTex     = commands[start].texture;
		bool          batchLighting  = commands[start].useLighting;
		BlendMode     batchBlend     = commands[start].blendMode;
		float         batchStrength  = commands[start].blendStrength;
		bool          batchAlphaTest = commands[start].enableAlphaTest;
		float         batchThreshold = commands[start].alphaThreshold;
		int end = start;
		while (end < (int)commands.size() &&
			   commands[end].texture        == currentTex &&
			   commands[end].useLighting    == batchLighting &&
			   commands[end].blendMode      == batchBlend &&
			   commands[end].blendStrength  == batchStrength &&
			   commands[end].enableAlphaTest == batchAlphaTest &&
			   commands[end].alphaThreshold  == batchThreshold) end++;
		obj->SetPipelineCommands(commandList_, &textureManager_, currentTex, batchBlend, batchStrength, batchAlphaTest, batchThreshold);
		commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(batchLighting));
		commandList_->SetGraphicsRoot32BitConstant(4, instanceBase + (UINT)start, 0);
		obj->Draw(commandList_, end - start, instanceBase + start);
		start = end;
	}

	commands.clear();
}

// Model/Sprite3D/Sprite2Dで共通の「1件描画」シーケンス。WVP/色/UVTransform等は
// 呼び出し側が事前にobjへ設定済みである前提で、パイプライン切り替え〜Draw呼び出しだけを担う
template<typename ObjectT>
void Renderer::IssueDrawCommand(ObjectT* obj, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold, bool useLighting, uint32_t instanceOffset) {
	obj->SetPipelineCommands(commandList_, &textureManager_, texture, blendMode, blendStrength, enableAlphaTest, alphaThreshold);
	commandList_->SetGraphicsRootConstantBufferView(3, light_.GetGPUAddress(useLighting));
	commandList_->SetGraphicsRoot32BitConstant(4, instanceOffset, 0);
	obj->Draw(commandList_, 1, instanceOffset);
}
