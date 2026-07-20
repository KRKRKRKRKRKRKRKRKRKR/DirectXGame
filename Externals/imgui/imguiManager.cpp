#include "ImGuiManager.h"
#include "imgui.h"
#include "imgui_internal.h" // DockBuilder系はinternal APIだが、初期デフォルトレイアウトの構築に使う
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "../../Engine/Graphics/DescriptorHeaps/DescriptorHeaps.h"
#include <cassert>

namespace {
	// ImGui自身のフォントテクスチャ用SRVを、ゲーム側テクスチャ（handle=0の白テクスチャ等、
	// SRVヒープindex 0〜kMaxTextureCount-1の帯）と衝突しない専用スロットから払い出す。
	// DescriptorHeaps::AllocateSRVIndexは解放を持たない使い捨てバンプアロケータのため、
	// Free側は何もしない（フォント1枚分のみが対象で、頻繁な確保/解放が発生せず実害はない）
	void ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
		auto* heaps = static_cast<DescriptorHeaps*>(info->UserData);
		uint32_t index = heaps->AllocateSRVIndex(1);
		*outCpu = heaps->GetCPUDescriptorHandle(heaps->GetSRVDescriptorHeap(), heaps->GetDescriptorSizeSRV(), index);
		*outGpu = heaps->GetGPUDescriptorHandle(heaps->GetSRVDescriptorHeap(), heaps->GetDescriptorSizeSRV(), index);
	}
	void ImGuiSrvFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {}
}

void ImGuiManager::Initialize(HWND hwnd, DirectXManager* dx) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hwnd);

	DescriptorHeaps* heaps = dx->GetDescriptorHeaps();
	ImGui_ImplDX12_InitInfo initInfo{};
	initInfo.Device               = dx->GetDevice();
	initInfo.CommandQueue         = dx->GetCommandQueue();
	initInfo.NumFramesInFlight    = 2;
	initInfo.RTVFormat            = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.SrvDescriptorHeap    = heaps->GetSRVDescriptorHeap();
	initInfo.UserData             = heaps;
	initInfo.SrvDescriptorAllocFn = ImGuiSrvAlloc;
	initInfo.SrvDescriptorFreeFn  = ImGuiSrvFree;
	bool ok = ImGui_ImplDX12_Init(&initInfo);
	assert(ok);

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// デフォルトのProggyCleanフォントはASCIIのみのため、日本語文字はグリフが無く"?"化する。
	// 日本語グリフ範囲込みでTTFを読み込み、ImGui::InputText等でも日本語入力・表示ができるようにする
	io.Fonts->AddFontFromFileTTF("Resources/Font/font.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	io.Fonts->Build();
}

void ImGuiManager::BeginFrame(bool showEditorPanels) {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 画面全体を覆うドッキング先（透明・タイトルバーなし）を用意する。
	// 各ウィンドウはここへドラッグ＆ドロップで自由にくっつけられる。
	// 固定IDにしておくことで、imgui.iniに保存済みのレイアウトがあるかどうかを
	// DockBuilderGetNode()で判定できるようにする（0=自動生成IDだと毎回変わってしまい判定できない）
	ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
	dockspaceId_ = dockspaceId; // GetSceneViewportRectが後でDockBuilderGetCentralNodeに使う
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	// このIDに対応する保存済みレイアウトがimgui.iniにまだ無い場合（＝初回、またはimgui.ini削除後）
	// だけ、右側にImGui専用の帯を作るデフォルトレイアウトを構築する。一度構築されればimgui.iniに
	// 保存されるため、以降の起動ではユーザーが後から調整した配置がそのまま復元される
	if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->Size);

		// Unityのデフォルトレイアウトを参考にした配置：
		//   +-----------+---------------------------+------------+
		//   | Hierarchy |   Scene（何もドッキング     | Inspector  |
		//   |           |    しない空の中央ノード）    |            |
		//   +-----------+---------------------------+------------+
		//   |         プロジェクト / ギズモ / その他タブ群          |
		//   +-------------------------------------------------------+
		// DockBuilderSplitNodeの戻り値には頼らず、分割後の両側を必ずout引数で受け取る
		// （戻り値をどちらか一方の代わりに使うと、片方をnullptrにした側と同じノードを指してしまい、
		// 意図せず2つの領域が同一ノードにエイリアスされてタブが混ざるバグになる）。
		// 「中央ノード（PassthruCentralNode）」の資格は、各分割で dir 側に指定しなかった
		// 反対側へ次々と引き継がれていく。最後まで一度もDockWindowしなかったcenterIdが
		// 3DシーンのPassthrough領域として残る
		ImGuiID topId, bottomId;
		ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Down, 0.28f, &bottomId, &topId);

		ImGuiID leftId, centerRightId;
		ImGui::DockBuilderSplitNode(topId, ImGuiDir_Left, 0.18f, &leftId, &centerRightId);

		ImGuiID rightId, centerId;
		ImGui::DockBuilderSplitNode(centerRightId, ImGuiDir_Right, 0.22f, &rightId, &centerId);

		ImGui::DockBuilderDockWindow("ヒエラルキー##Hierarchy", leftId);
		ImGui::DockBuilderDockWindow("インスペクター##Inspector", rightId);
		ImGui::DockBuilderDockWindow("プロジェクト##Project", bottomId);
		ImGui::DockBuilderDockWindow("ギズモ##Gizmo", bottomId);
		ImGui::DockBuilderDockWindow("カメラ##Camera", bottomId);
		ImGui::DockBuilderDockWindow("FPS", bottomId);
		ImGui::DockBuilderDockWindow("オーディオ##Audio", bottomId);
		ImGui::DockBuilderDockWindow("ライティング##Lighting", bottomId);
		ImGui::DockBuilderDockWindow("メッシュ設定##Mesh Settings", bottomId);

		ImGui::DockBuilderFinish(dockspaceId);
	}

	// NoDockingOverCentralNodeで、ゲーム画面用に残した中央ノードへ後から別のウィンドウが
	// ドッキングされてしまわないようにする。showEditorPanels=falseのときはドックスペース自体を
	// 提出しない＝どのパネルウィンドウも描画対象にならず、画面全体がそのままゲーム画面になる
	if (showEditorPanels) {
		ImGui::DockSpaceOverViewport(dockspaceId, viewport,
			ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);
	}
}

void ImGuiManager::GetSceneViewportRect(float& x, float& y, float& width, float& height) const {
	ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockspaceId_);
	if (central) {
		x = central->Pos.x;
		y = central->Pos.y;
		width = central->Size.x;
		height = central->Size.y;
	} else {
		// まだBeginFrame()が一度も呼ばれていない等、取得できない場合は呼び出し側が
		// フルウィンドウ等へフォールバックできるよう0を返す
		x = y = width = height = 0.0f;
	}
}

void ImGuiManager::EndFrame(DirectXManager* dx) {
	ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dx->GetCommandList());

}

void ImGuiManager::Finalize() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}