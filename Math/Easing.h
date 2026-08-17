#pragma once

// https://easings.net/ja の全イージング関数の実装。
// 入力t（0.0〜1.0の経過割合）を受け取り、変換後の割合（0.0〜1.0が基本、Back/Elastic系は
// 一時的に範囲外の値を取ることがある）を返す。呼び出し側はこの戻り値を使って
// Lerp(start, end, easedT) のように補間する
namespace Easing {

enum class Type {
	kLinear,
	kInSine, kOutSine, kInOutSine,
	kInQuad, kOutQuad, kInOutQuad,
	kInCubic, kOutCubic, kInOutCubic,
	kInQuart, kOutQuart, kInOutQuart,
	kInQuint, kOutQuint, kInOutQuint,
	kInExpo, kOutExpo, kInOutExpo,
	kInCirc, kOutCirc, kInOutCirc,
	kInBack, kOutBack, kInOutBack,
	kInElastic, kOutElastic, kInOutElastic,
	kInBounce, kOutBounce, kInOutBounce,

	kCount, // 要素数。配列サイズ/ループ境界にのみ使う番兵
};

// Inspectorのコンボ等に使う表示名一覧（Type と同じ並び順）
const char* const* GetTypeNames();

// tにイージング関数を適用した値を返す
float Apply(Type type, float t);

} // namespace Easing
