#pragma once

namespace cfg {
struct Range { const wchar_t* key; double min, max, def; };

inline constexpr Range DynamicWidth{ L"DynamicWidth", 0, 1, 1 };
inline constexpr Range Width{ L"Width", 200, 3000, 900 };
inline constexpr Range Padding{ L"Padding", 0, 60, 14 };
inline constexpr Range MarginV{ L"MarginV", 0, 16, 4 };
inline constexpr Range CornerRadius{ L"CornerRadius", 0, 30, 20 };
inline constexpr Range OffsetY{ L"OffsetY", -4, 4, 0 };
inline constexpr Range FullWidth{ L"FullWidth", 0, 1, 0 };
inline constexpr Range HighlightRadius{ L"HighlightRadius", 0, 20, 18 };

inline constexpr Range BlurAmount{ L"BlurAmount", 0, 4, 2 };
inline constexpr Range Refraction{ L"Refraction", 0, 0.4, 0.2 };
inline constexpr Range Specular{ L"Specular", 0, 0.14, 0.07 };
inline constexpr Range BorderGlow{ L"BorderGlow", 0, 0.14, 0.07 };
inline constexpr Range TintOpacity{ L"TintOpacity", 0, 1, 0.5 };
inline constexpr Range PanelTintOpacity{ L"PanelTintOpacity", 0, 1, 0.5 };
inline constexpr const wchar_t* TintColorKey = L"TintColor";
inline constexpr unsigned TintColorDef = 0x202020;

inline constexpr Range AnimationTime{ L"AnimationTime", 0, 600, 150 };
inline constexpr Range RimWidth{ L"RimWidth", 0.5, 4, 1.5 };
inline constexpr Range Shadow{ L"Shadow", 0, 1, 0 };
inline constexpr Range HoverStrength{ L"HoverStrength", 0, 3, 1 };
inline constexpr Range ShowIndicator{ L"ShowIndicator", 0, 1, 1 };
inline constexpr Range ClockSize{ L"ClockSize", 0.7, 1.4, 1 };
inline constexpr Range ClockBold{ L"ClockBold", 0, 1, 0 };
inline constexpr int Stock = -1;
inline constexpr const wchar_t* RimColorKey = L"RimColor";
inline constexpr int RimColorDef = 0xFFFFFF;
inline constexpr const wchar_t* HoverColorKey = L"HoverColor";
inline constexpr const wchar_t* IndicatorColorKey = L"IndicatorColor";
inline constexpr const wchar_t* ClockColorKey = L"ClockColor";

constexpr double RimAlpha(Range r, double v) { return v / r.def * 0.2; }
constexpr double RimMid(double specular, double glow) { return (RimAlpha(Specular, specular) + RimAlpha(BorderGlow, glow)) / 2; }

constexpr bool Inside(Range r) { return r.min <= r.def && r.def <= r.max; }
constexpr bool Middle(Range r) { double d = r.def * 2 - (r.min + r.max); return d < 1e-9 && d > -1e-9; }
static_assert(Inside(DynamicWidth) && Inside(Width) && Inside(Padding) && Inside(MarginV) && Inside(CornerRadius) &&
              Inside(FullWidth) && Inside(HighlightRadius) && Inside(OffsetY) && Inside(AnimationTime) &&
              Inside(RimWidth) && Inside(Shadow) && Inside(HoverStrength) && Inside(ShowIndicator) && Inside(ClockSize) &&
              Inside(ClockBold));
static_assert(Middle(BlurAmount) && Middle(Refraction) && Middle(Specular) && Middle(BorderGlow) && Middle(TintOpacity) &&
              Middle(PanelTintOpacity),
              "the reference glass must sit in the middle of each slider");

}
