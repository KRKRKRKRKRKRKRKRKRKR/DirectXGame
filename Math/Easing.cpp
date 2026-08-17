#include "Easing.h"
#include <cmath>

namespace Easing {

namespace {
	constexpr float kPi = 3.14159265358979323846f;

	float InSine(float t)    { return 1.0f - cosf((t * kPi) / 2.0f); }
	float OutSine(float t)   { return sinf((t * kPi) / 2.0f); }
	float InOutSine(float t) { return -(cosf(kPi * t) - 1.0f) / 2.0f; }

	float InQuad(float t)    { return t * t; }
	float OutQuad(float t)   { return 1.0f - (1.0f - t) * (1.0f - t); }
	float InOutQuad(float t) { return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f; }

	float InCubic(float t)    { return t * t * t; }
	float OutCubic(float t)   { return 1.0f - powf(1.0f - t, 3.0f); }
	float InOutCubic(float t) { return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f; }

	float InQuart(float t)    { return t * t * t * t; }
	float OutQuart(float t)   { return 1.0f - powf(1.0f - t, 4.0f); }
	float InOutQuart(float t) { return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 4.0f) / 2.0f; }

	float InQuint(float t)    { return t * t * t * t * t; }
	float OutQuint(float t)   { return 1.0f - powf(1.0f - t, 5.0f); }
	float InOutQuint(float t) { return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 5.0f) / 2.0f; }

	float InExpo(float t)  { return t <= 0.0f ? 0.0f : powf(2.0f, 10.0f * t - 10.0f); }
	float OutExpo(float t) { return t >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t); }
	float InOutExpo(float t) {
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		return t < 0.5f ? powf(2.0f, 20.0f * t - 10.0f) / 2.0f : (2.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;
	}

	float InCirc(float t)    { return 1.0f - sqrtf(1.0f - powf(t, 2.0f)); }
	float OutCirc(float t)   { return sqrtf(1.0f - powf(t - 1.0f, 2.0f)); }
	float InOutCirc(float t) {
		return t < 0.5f
			? (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f
			: (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
	}

	float InBack(float t) {
		constexpr float c1 = 1.70158f;
		constexpr float c3 = c1 + 1.0f;
		return c3 * t * t * t - c1 * t * t;
	}
	float OutBack(float t) {
		constexpr float c1 = 1.70158f;
		constexpr float c3 = c1 + 1.0f;
		return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
	}
	float InOutBack(float t) {
		constexpr float c1 = 1.70158f;
		constexpr float c2 = c1 * 1.525f;
		return t < 0.5f
			? (powf(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
			: (powf(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
	}

	float InElastic(float t) {
		constexpr float c4 = (2.0f * kPi) / 3.0f;
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * c4);
	}
	float OutElastic(float t) {
		constexpr float c4 = (2.0f * kPi) / 3.0f;
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
	}
	float InOutElastic(float t) {
		constexpr float c5 = (2.0f * kPi) / 4.5f;
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		return t < 0.5f
			? -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f
			: (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
	}

	float OutBounce(float t) {
		constexpr float n1 = 7.5625f;
		constexpr float d1 = 2.75f;
		if (t < 1.0f / d1) {
			return n1 * t * t;
		} else if (t < 2.0f / d1) {
			t -= 1.5f / d1;
			return n1 * t * t + 0.75f;
		} else if (t < 2.5f / d1) {
			t -= 2.25f / d1;
			return n1 * t * t + 0.9375f;
		} else {
			t -= 2.625f / d1;
			return n1 * t * t + 0.984375f;
		}
	}
	float InBounce(float t) { return 1.0f - OutBounce(1.0f - t); }
	float InOutBounce(float t) {
		return t < 0.5f
			? (1.0f - OutBounce(1.0f - 2.0f * t)) / 2.0f
			: (1.0f + OutBounce(2.0f * t - 1.0f)) / 2.0f;
	}

	// Type と同じ並び順（GetTypeNamesとも対応させる）
	constexpr const char* kTypeNames[] = {
		"Linear",
		"InSine", "OutSine", "InOutSine",
		"InQuad", "OutQuad", "InOutQuad",
		"InCubic", "OutCubic", "InOutCubic",
		"InQuart", "OutQuart", "InOutQuart",
		"InQuint", "OutQuint", "InOutQuint",
		"InExpo", "OutExpo", "InOutExpo",
		"InCirc", "OutCirc", "InOutCirc",
		"InBack", "OutBack", "InOutBack",
		"InElastic", "OutElastic", "InOutElastic",
		"InBounce", "OutBounce", "InOutBounce",
	};
	static_assert(sizeof(kTypeNames) / sizeof(kTypeNames[0]) == static_cast<size_t>(Type::kCount),
		"kTypeNamesとTypeの要素数が一致していません");
}

const char* const* GetTypeNames() {
	return kTypeNames;
}

float Apply(Type type, float t) {
	switch (type) {
		case Type::kLinear:      return t;
		case Type::kInSine:      return InSine(t);
		case Type::kOutSine:     return OutSine(t);
		case Type::kInOutSine:   return InOutSine(t);
		case Type::kInQuad:      return InQuad(t);
		case Type::kOutQuad:     return OutQuad(t);
		case Type::kInOutQuad:   return InOutQuad(t);
		case Type::kInCubic:     return InCubic(t);
		case Type::kOutCubic:    return OutCubic(t);
		case Type::kInOutCubic:  return InOutCubic(t);
		case Type::kInQuart:     return InQuart(t);
		case Type::kOutQuart:    return OutQuart(t);
		case Type::kInOutQuart:  return InOutQuart(t);
		case Type::kInQuint:     return InQuint(t);
		case Type::kOutQuint:    return OutQuint(t);
		case Type::kInOutQuint:  return InOutQuint(t);
		case Type::kInExpo:      return InExpo(t);
		case Type::kOutExpo:     return OutExpo(t);
		case Type::kInOutExpo:   return InOutExpo(t);
		case Type::kInCirc:      return InCirc(t);
		case Type::kOutCirc:     return OutCirc(t);
		case Type::kInOutCirc:   return InOutCirc(t);
		case Type::kInBack:      return InBack(t);
		case Type::kOutBack:     return OutBack(t);
		case Type::kInOutBack:   return InOutBack(t);
		case Type::kInElastic:   return InElastic(t);
		case Type::kOutElastic:  return OutElastic(t);
		case Type::kInOutElastic: return InOutElastic(t);
		case Type::kInBounce:    return InBounce(t);
		case Type::kOutBounce:   return OutBounce(t);
		case Type::kInOutBounce: return InOutBounce(t);
		default:                 return t;
	}
}

} // namespace Easing
