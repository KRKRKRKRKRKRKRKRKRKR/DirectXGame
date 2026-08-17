#pragma once
#include <string>

// プロジェクトパネルが走査したアセット1件分（パス＋表示名）。SceneBase::projectAudioClips_と
// HitSoundComponentのコンボ選択の両方で共有するため、TextureEntry.hと同じ考え方でEngine側の
// 共有ヘッダに置く（元々はSceneBase.h内のprivateなネスト構造体だったが、HitSoundComponentが
// コンストラクタ引数として受け取る必要が出たためここへ切り出した）
struct ProjectAssetEntry {
	std::string path;
	std::string displayName;
};
