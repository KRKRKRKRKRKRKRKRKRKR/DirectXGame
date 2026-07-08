#pragma once

// エンジンが持つ全コンポーネント種をComponentRegistryへ登録する。JSONのSave/Loadを使う前に
// 一度だけ呼べばよい（Game::Initialize()から呼ぶ）。「今どのコンポーネントが保存/復元対応か」
// をこの1ファイルを見れば把握できるようにするための集約ポイント
void RegisterEngineComponents();
