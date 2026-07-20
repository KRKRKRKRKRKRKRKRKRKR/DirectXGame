#pragma once
#include <windows.h>
#include "../../Engine/Graphics/Renderer/DirectXManager.h"

class ImGuiManager {

public:
	ImGuiManager() = default;
	~ImGuiManager() = default;

	void Initialize(HWND hwnd, DirectXManager* dx);
	// showEditorPanels=falseのときはドックスペースを提出しない（＝Hierarchy/Inspector/Project/Gizmo等
	// どのパネルウィンドウも描画されず、3D描画がウィンドウ全体に広がる。Engine::Update参照）
	void BeginFrame(bool showEditorPanels);
	void EndFrame(DirectXManager* dx);
	void Finalize();

	// ドック中央ノード（Sceneビュー）の現在の画面座標矩形を返す。BeginFrame()呼び出し後
	// （毎フレームのドック構築後）にのみ有効。取得できない場合はすべて0を返す
	void GetSceneViewportRect(float& x, float& y, float& width, float& height) const;

private:
	// 型はImGuiID（=unsigned int）だが、このヘッダにimgui.hを含めずに済むよう素のunsigned intで持つ
	unsigned int dockspaceId_ = 0;
};