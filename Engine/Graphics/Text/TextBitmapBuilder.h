#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// フォント＋文字列から1枚の合成RGBAビットマップ（テキストブロック全体を1枚にまとめたもの）を作る。
// R=G=B=255固定、A=カバレッジ（アンチエイリアス濃度）とし、既存のSprite用シェーダー・
// アルファブレンド・RenderComponentBase::colorティントをそのまま使えるようにしている。
struct TextBitmap {
	std::vector<uint8_t> rgbaPixels;
	uint32_t width  = 0;
	uint32_t height = 0;
};

class TextBitmapBuilder {
public:
	TextBitmapBuilder();
	~TextBitmapBuilder();

	// フォントファイル（.ttf/.otf）をメモリに読み込む
	bool LoadFont(const std::string& fontPath);

	// textは改行(U'\n')込みのUnicodeコードポイント列。pixelHeightはフォントサイズ(px)、
	// lineSpacingは行送りの倍率（1.0で標準行間）
	bool Build(const std::vector<char32_t>& text, float pixelHeight, float lineSpacing, TextBitmap& out) const;

	// キャンバスサイズを固定して描画する版。文字列に関わらず常に同じ(canvasWidth, canvasHeight)の
	// ビットマップを返すため、HUD等「毎フレーム内容だけ変わるテクスチャ」をGPUリソースを作り直さず
	// TextureManager::UpdatePixelsで更新する用途に使う。文字列がキャンバスより大きい場合ははみ出た
	// 分をクリップする（文字送り自体はクリップしない）
	bool BuildFixed(const std::vector<char32_t>& text, float pixelHeight, float lineSpacing,
		uint32_t canvasWidth, uint32_t canvasHeight, TextBitmap& out) const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
