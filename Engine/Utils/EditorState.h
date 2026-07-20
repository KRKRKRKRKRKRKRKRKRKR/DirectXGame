#pragma once

// エディタ用ImGuiパネル（Hierarchy/Inspector/Project/Gizmo等）とSceneの自由カメラ編集を
// 表示/操作可能にするかどうかのグローバルな状態。F11キーで切り替える（Engine::Update参照）
class EditorState {
public:
	static EditorState& GetInstance() {
		static EditorState instance;
		return instance;
	}
	EditorState(const EditorState&) = delete;
	EditorState& operator=(const EditorState&) = delete;

	bool IsUiVisible() const { return uiVisible_; }
	void ToggleUiVisible() { uiVisible_ = !uiVisible_; }

private:
	EditorState() = default;
	// Releaseビルドは既定でUIを隠し、実際のゲーム画面だけが最初から見える状態で起動する。
	// Debugビルドは今まで通りエディタUIが見えた状態で起動する（どちらもF11で手動切り替え可能）
#ifdef NDEBUG
	bool uiVisible_ = false;
#else
	bool uiVisible_ = true;
#endif
};
