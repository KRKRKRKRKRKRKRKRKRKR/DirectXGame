#pragma once
#include <d3d12.h>
#include "BlendMode.h"

// Cube/Sphere/Triangle/Sprite/Model の SetPipelineCommands が共通で行う
// RootSignature/PSO切り替え + テクスチャ・WVP・色のディスクリプタテーブル設定 +
// ブレンド/αTest情報(b2)のRoot Constants設定をまとめたヘルパー。
// ボーン行列パレット(t3)などモデル固有の追加バインドは呼び出し側で別途行う。
namespace PipelineCommandHelper {

	inline void ApplyCommon(
		ID3D12GraphicsCommandList* commandList,
		ID3D12RootSignature* rootSignature,
		ID3D12PipelineState* pipelineState,
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE wvpSrvHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle,
		BlendMode blendMode,
		float blendStrength,
		bool enableAlphaTest,
		float alphaThreshold) {

		commandList->SetGraphicsRootSignature(rootSignature);
		commandList->SetPipelineState(pipelineState);

		commandList->SetGraphicsRootDescriptorTable(0, textureSrvHandle); // t0: テクスチャ
		commandList->SetGraphicsRootDescriptorTable(1, wvpSrvHandle);     // t1: WVP
		commandList->SetGraphicsRootDescriptorTable(2, colorSrvHandle);   // t2: 色

		commandList->SetGraphicsRoot32BitConstant(5, static_cast<UINT>(blendMode), 0);              // b2.x: blendMode
		commandList->SetGraphicsRoot32BitConstant(5, *reinterpret_cast<const UINT*>(&blendStrength), 1); // b2.y: blendStrength
		commandList->SetGraphicsRoot32BitConstant(5, static_cast<UINT>(enableAlphaTest), 2);         // b2.z: enableAlphaTest
		commandList->SetGraphicsRoot32BitConstant(5, *reinterpret_cast<const UINT*>(&alphaThreshold), 3); // b2.w: alphaThreshold
	}
}
