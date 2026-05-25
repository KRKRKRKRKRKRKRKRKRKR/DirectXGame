#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "../DirectXGame/Math/MathTypes.h"

using Microsoft::WRL::ComPtr;

// 描画可能なオブジェクトの基底クラス
class IDrawable {
public:
	virtual ~IDrawable() = default;

	// GPU コマンドリストへの描画命令を記録
	virtual void Draw(ID3D12GraphicsCommandList* commandList) = 0;

	// WVP 行列インデックスを取得（行列プーリング用）
	virtual uint32_t GetWvpIndex() const = 0;

	// 描画パイプラインの設定を取得
	virtual ID3D12RootSignature* GetRootSignature() const = 0;
	virtual ID3D12PipelineState* GetPipelineState() const = 0;
};
