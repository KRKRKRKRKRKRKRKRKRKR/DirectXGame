#pragma once
#include "Renderer.h"

// モデルの遅延ロード状態（試したか／ハンドル）をまとめて持つ小さなヘルパー。
// IComponentのコンストラクタはRendererを受け取れないため、「初回Draw/Update時に
// 1度だけLoadModelを試みる」という同じ3変数パターン（試行済みフラグ／ロード成否フラグ／
// ハンドル）がClickHintMarkerComponentとReflexPlayerComponentに独立して複製されていた
// ものを1箇所にまとめた。Get()を呼ぶだけで「未ロードなら試す→ハンドルを返す」を行う。
// 描画コンポーネントのDraw()がconstであることが多いため、mutableな内部状態を
// カプセル化する目的も兼ねる
class LazyModelHandle {
public:
	LazyModelHandle(const char* directory, const char* filename)
		: directory_(directory), filename_(filename) {}

	// 未ロードなら1度だけLoadModelを試みてから、現在のハンドルを返す。
	// 失敗時（LoadModelが0を返す場合）も再試行はしない（従来のtryLoadCircleModel_と同じ挙動）
	Renderer::ModelHandle Get(Renderer* renderer) const {
		if (!tried_) {
			tried_ = true;
			handle_ = renderer->LoadModel(directory_, filename_);
		}
		return handle_;
	}

private:
	const char* directory_;
	const char* filename_;
	mutable bool tried_ = false;
	mutable Renderer::ModelHandle handle_ = 0;
};
