#pragma once

// ブレンドモード：ピクセルシェーダーが出した色(Src)と、
// 描画先に既に書き込まれている色(Dest)をどう合成するか
// D3D12ではBlendStateはPSOに焼き込まれる値なので、切り替えるには
// モードの数だけPSOを作っておき、描画直前にSetPipelineStateで差し替える
enum class BlendMode {
	kNone,     // ブレンドしない。Srcでそのまま上書き（アルファ無視）
	kNormal,   // 通常のアルファブレンド。Src*a + Dest*(1-a)
	kAdd,      // 加算。Src*a + Dest*1（光・炎などを明るく重ねる）
	kSubtract, // 減算。Dest*1 - Src*a
	kMultiply, // 乗算。Dest*Src（影・汚れなど暗くする）
	kScreen,   // スクリーン。Src*1 + Dest*(1-Src)（発光・光漏れなど）

	kCount, // 要素数（配列サイズとして使う番兵）
};
