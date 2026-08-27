#include "TextBitmapBuilder.h"

// imguiが同梱しているstb_truetypeをそのまま流用する。imgui_draw.cppもSTBTT_STATIC付きで
// 実装を生成しているため、こちらでも同じマクロで実装を生成すれば関数は内部リンケージになり、
// 別TUで二重定義（リンクエラー）にはならない
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../Externals/imgui/imstb_truetype.h"

#include <algorithm>
#include <cmath>
#include <fstream>

struct TextBitmapBuilder::Impl {
	std::vector<unsigned char> fontFileData;
	stbtt_fontinfo             fontInfo{};
	bool                       loaded = false;
};

namespace {

// キャンバス端まで文字がぴったり詰まっていると、Samplerが D3D12_TEXTURE_ADDRESS_MODE_WRAP の
// ため、UV=1.0付近をバイリニアフィルタでサンプリングした際に反対端（UV=0付近）のピクセルと
// 混ざり、文字の一部が逆側の端ににじんで見える。文字列の周囲に余白を持たせることでこれを防ぐ
constexpr int kGlyphPaddingPx = 2;

// キャンバスサイズ（out.width/out.height）が既に決まっている前提で、各文字をラスタライズして
// キャンバスへ書き込む（Build/BuildFixed共通の2パス目）。キャンバス外にはみ出た分はクリップする。
// padding分だけ描画開始位置をキャンバス内側にずらす（kGlyphPaddingPx、呼び出し側でキャンバス
// サイズにも同じ分を足しておく必要がある）
void RenderGlyphsInto(const stbtt_fontinfo& font, const std::vector<char32_t>& text,
	float scale, float lineAdvance, float baselineFirst, TextBitmap& out, int paddingX = 0, int paddingY = 0) {
	std::vector<unsigned char> glyphBuffer;
	float cursorX = 0.0f;
	float cursorY = baselineFirst;
	for (char32_t cp : text) {
		if (cp == U'\n') {
			cursorX  = 0.0f;
			cursorY += lineAdvance;
			continue;
		}

		int advanceWidth = 0, leftSideBearing = 0;
		stbtt_GetCodepointHMetrics(&font, static_cast<int>(cp), &advanceWidth, &leftSideBearing);

		int ix0 = 0, iy0 = 0, ix1 = 0, iy1 = 0;
		stbtt_GetCodepointBitmapBox(&font, static_cast<int>(cp), scale, scale, &ix0, &iy0, &ix1, &iy1);
		int glyphWidth  = ix1 - ix0;
		int glyphHeight = iy1 - iy0;

		if (glyphWidth > 0 && glyphHeight > 0) {
			glyphBuffer.assign(static_cast<size_t>(glyphWidth) * glyphHeight, 0);
			stbtt_MakeCodepointBitmap(&font, glyphBuffer.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, static_cast<int>(cp));

			int destX = static_cast<int>(cursorX) + ix0 + paddingX;
			int destY = static_cast<int>(cursorY) + iy0 + paddingY;
			for (int y = 0; y < glyphHeight; ++y) {
				int py = destY + y;
				if (py < 0 || py >= static_cast<int>(out.height)) continue;
				for (int x = 0; x < glyphWidth; ++x) {
					int px = destX + x;
					if (px < 0 || px >= static_cast<int>(out.width)) continue;
					unsigned char coverage = glyphBuffer[static_cast<size_t>(y) * glyphWidth + x];
					if (coverage == 0) continue;
					size_t idx = (static_cast<size_t>(py) * out.width + static_cast<size_t>(px)) * 4;
					out.rgbaPixels[idx + 0] = 255;
					out.rgbaPixels[idx + 1] = 255;
					out.rgbaPixels[idx + 2] = 255;
					out.rgbaPixels[idx + 3] = coverage;
				}
			}
		}

		cursorX += static_cast<float>(advanceWidth) * scale;
	}
}

} // namespace

TextBitmapBuilder::TextBitmapBuilder() : impl_(std::make_unique<Impl>()) {}
TextBitmapBuilder::~TextBitmapBuilder() = default;

bool TextBitmapBuilder::LoadFont(const std::string& fontPath) {
	std::ifstream file(fontPath, std::ios::binary);
	if (!file.is_open()) return false;

	file.seekg(0, std::ios::end);
	auto size = static_cast<size_t>(file.tellg());
	file.seekg(0, std::ios::beg);

	impl_->fontFileData.resize(size);
	file.read(reinterpret_cast<char*>(impl_->fontFileData.data()), static_cast<std::streamsize>(size));
	if (!file) return false;

	impl_->loaded = stbtt_InitFont(&impl_->fontInfo, impl_->fontFileData.data(), 0) != 0;
	return impl_->loaded;
}

namespace {

// Build/MeasureText共通の1パス目：改行を反映したテキスト全体の占有サイズ(px、kGlyphPaddingPx込み)を求める。
// scale/lineAdvance/baselineFirstはstbtt_ScaleForPixelHeight等から呼び出し側が計算済みのものを渡す
void ComputeTextExtent(const stbtt_fontinfo& font, const std::vector<char32_t>& text,
	float scale, float lineAdvance, int ascent, int descent, uint32_t& outWidth, uint32_t& outHeight) {
	float cursorX  = 0.0f;
	float maxWidth = 0.0f;
	int   lineCount = 1;
	for (char32_t cp : text) {
		if (cp == U'\n') {
			maxWidth = std::max(maxWidth, cursorX);
			cursorX  = 0.0f;
			++lineCount;
			continue;
		}
		int advanceWidth = 0, leftSideBearing = 0;
		stbtt_GetCodepointHMetrics(&font, static_cast<int>(cp), &advanceWidth, &leftSideBearing);
		cursorX += static_cast<float>(advanceWidth) * scale;
	}
	maxWidth = std::max(maxWidth, cursorX);

	outWidth  = static_cast<uint32_t>(std::ceil(maxWidth))  + kGlyphPaddingPx * 2;
	outHeight = static_cast<uint32_t>(std::ceil(lineAdvance * static_cast<float>(lineCount - 1) + static_cast<float>(ascent - descent) * scale)) + kGlyphPaddingPx * 2;
}

} // namespace

bool TextBitmapBuilder::Build(const std::vector<char32_t>& text, float pixelHeight, float lineSpacing, TextBitmap& out) const {
	if (!impl_->loaded) return false;

	const stbtt_fontinfo& font = impl_->fontInfo;
	float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);
	int ascent = 0, descent = 0, lineGap = 0;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
	float lineAdvance   = (static_cast<float>(ascent - descent + lineGap)) * scale * lineSpacing;
	float baselineFirst = static_cast<float>(ascent) * scale;

	uint32_t width = 0, height = 0;
	ComputeTextExtent(font, text, scale, lineAdvance, ascent, descent, width, height);
	if (width <= static_cast<uint32_t>(kGlyphPaddingPx * 2) || height <= static_cast<uint32_t>(kGlyphPaddingPx * 2)) return false;

	out.width  = width;
	out.height = height;
	out.rgbaPixels.assign(static_cast<size_t>(width) * height * 4, 0);

	RenderGlyphsInto(font, text, scale, lineAdvance, baselineFirst, out, kGlyphPaddingPx, kGlyphPaddingPx);
	return true;
}

bool TextBitmapBuilder::MeasureText(const std::vector<char32_t>& text, float pixelHeight, float lineSpacing,
	uint32_t& outWidth, uint32_t& outHeight) const {
	if (!impl_->loaded || text.empty()) return false;

	const stbtt_fontinfo& font = impl_->fontInfo;
	float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);
	int ascent = 0, descent = 0, lineGap = 0;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
	float lineAdvance = (static_cast<float>(ascent - descent + lineGap)) * scale * lineSpacing;

	ComputeTextExtent(font, text, scale, lineAdvance, ascent, descent, outWidth, outHeight);
	return outWidth > static_cast<uint32_t>(kGlyphPaddingPx * 2) && outHeight > static_cast<uint32_t>(kGlyphPaddingPx * 2);
}

bool TextBitmapBuilder::BuildFixed(const std::vector<char32_t>& text, float pixelHeight, float lineSpacing,
	uint32_t canvasWidth, uint32_t canvasHeight, TextBitmap& out) const {
	if (!impl_->loaded) return false;
	if (canvasWidth == 0 || canvasHeight == 0) return false;

	const stbtt_fontinfo& font = impl_->fontInfo;
	float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);
	int ascent = 0, descent = 0, lineGap = 0;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
	float lineAdvance   = (static_cast<float>(ascent - descent + lineGap)) * scale * lineSpacing;
	float baselineFirst = static_cast<float>(ascent) * scale;

	out.width  = canvasWidth;
	out.height = canvasHeight;
	out.rgbaPixels.assign(static_cast<size_t>(canvasWidth) * canvasHeight * 4, 0);

	// 左上にkGlyphPaddingPx分の余白を持たせ、文字がキャンバス左端に張り付かないようにする
	// （WRAPサンプラーによる端のにじみ対策。右/下端は呼び出し側のcanvasWidth/Heightに
	// 十分な余裕がある前提で、ここでは開始位置のみ調整する）
	RenderGlyphsInto(font, text, scale, lineAdvance, baselineFirst, out, kGlyphPaddingPx, kGlyphPaddingPx);
	return true;
}
