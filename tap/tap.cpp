#include <windows.h>
#include <unknwn.h>
#include <ocidl.h>
#include <xamlOM.h>
#include <d2d1effects.h>
#include <windows.graphics.effects.interop.h>
#include <map>
#include "../shared/opal_config.h"
#include "../shared/opal_app.h"
#undef GetCurrentTime
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.System.Power.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace wf = winrt::Windows::Foundation;
namespace wge = winrt::Windows::Graphics::Effects;
namespace wuc = winrt::Windows::UI::Composition;
namespace wux = winrt::Windows::UI::Xaml;
using wux::Media::VisualTreeHelper;
using Rect = wux::Shapes::Rectangle;
using PV = wf::PropertyValue;

static HMODULE g_mod;
static std::wstring g_ini;

static void Log(std::wstring const& s)
{
    auto path = g_ini.substr(0, g_ini.rfind(L'\\') + 1) + L"OpalTap.log";
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t ts[48]; swprintf_s(ts, L"%02d:%02d:%02d.%03d [%lu:%lu] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentProcessId(), GetCurrentThreadId());
    auto line = winrt::to_string(ts + s) + "\r\n";
    DWORD n; WriteFile(f, line.data(), (DWORD)line.size(), &n, nullptr);
    CloseHandle(f);
}

static std::wstring Hex(HRESULT hr) { wchar_t b[16]; swprintf_s(b, L"0x%08X", (unsigned)hr); return b; }

struct Config {
    bool dynamicWidth; double width, padding, marginV, radius;
    float blur, refraction; double specular, glow, tintOpacity;
    winrt::Windows::UI::Color tint;
    bool fullWidth; double hlRadius;
    double panelTint;
    double offsetY;
    double anim, rimWidth, shadow, hover, clockSize;
    int rimColor, hoverColor, indColor, clockColor;
    bool indicator, clockBold;
};

static double ReadF(const wchar_t* key, double def)
{
    wchar_t b[64];
    GetPrivateProfileStringW(L"Taskbar", key, L"", b, 64, g_ini.c_str());
    return *b ? _wtof(b) : def;
}

static double Read(cfg::Range r) { return std::clamp(ReadF(r.key, r.def), r.min, r.max); }

static int ReadColor(const wchar_t* key, int def)
{
    wchar_t b[16];
    GetPrivateProfileStringW(L"Taskbar", key, L"", b, 16, g_ini.c_str());
    return *b ? int(wcstoul(b + (*b == L'#'), nullptr, 16) & 0xFFFFFF) : def;
}

static Config LoadConfig()
{
    unsigned rgb = ReadColor(cfg::TintColorKey, cfg::TintColorDef);
    Config c{ Read(cfg::DynamicWidth) != 0, ReadF(cfg::Width.key, cfg::Width.def), Read(cfg::Padding),
             Read(cfg::MarginV), Read(cfg::CornerRadius),
             (float)Read(cfg::BlurAmount), (float)Read(cfg::Refraction),
             Read(cfg::Specular), Read(cfg::BorderGlow), Read(cfg::TintOpacity),
             { 255, BYTE(rgb >> 16), BYTE(rgb >> 8), BYTE(rgb) },
             Read(cfg::FullWidth) != 0, Read(cfg::HighlightRadius), Read(cfg::PanelTintOpacity), Read(cfg::OffsetY) };
    c.anim = Read(cfg::AnimationTime);
    c.rimWidth = Read(cfg::RimWidth);
    c.shadow = Read(cfg::Shadow);
    c.hover = Read(cfg::HoverStrength);
    c.clockSize = Read(cfg::ClockSize);
    c.rimColor = ReadColor(cfg::RimColorKey, cfg::RimColorDef);
    c.hoverColor = ReadColor(cfg::HoverColorKey, cfg::Stock);
    c.indColor = ReadColor(cfg::IndicatorColorKey, cfg::Stock);
    c.clockColor = ReadColor(cfg::ClockColorKey, cfg::Stock);
    c.indicator = Read(cfg::ShowIndicator) != 0;
    c.clockBold = Read(cfg::ClockBold) != 0;
    return c;
}

static std::mutex g_lock;
static Config g_cfg;

struct Effect : winrt::implements<Effect, wge::IGraphicsEffect, wge::IGraphicsEffectSource,
                                  ABI::Windows::Graphics::Effects::IGraphicsEffectD2D1Interop> {
    GUID id; std::vector<wf::IInspectable> props; std::vector<wge::IGraphicsEffectSource> srcs;
    winrt::hstring name;

    Effect(GUID i, std::vector<wf::IInspectable> p, std::vector<wge::IGraphicsEffectSource> s = {})
        : id(i), props(std::move(p)), srcs(std::move(s)) {}

    winrt::hstring Name() { return name; }
    void Name(winrt::hstring const& n) { name = n; }

    IFACEMETHODIMP GetEffectId(GUID* g) noexcept override { *g = id; return S_OK; }
    IFACEMETHODIMP GetNamedPropertyMapping(LPCWSTR, UINT*, ABI::Windows::Graphics::Effects::GRAPHICS_EFFECT_PROPERTY_MAPPING*) noexcept override
    {
        return E_INVALIDARG;
    }
    IFACEMETHODIMP GetPropertyCount(UINT* c) noexcept override { *c = (UINT)props.size(); return S_OK; }
    IFACEMETHODIMP GetProperty(UINT i, ABI::Windows::Foundation::IPropertyValue** v) noexcept override
    {
        if (i >= props.size()) return E_INVALIDARG;
        *v = props[i].as<ABI::Windows::Foundation::IPropertyValue>().detach();
        return S_OK;
    }
    IFACEMETHODIMP GetSource(UINT i, ABI::Windows::Graphics::Effects::IGraphicsEffectSource** s) noexcept override
    {
        if (i >= srcs.size()) return E_INVALIDARG;
        *s = srcs[i].as<ABI::Windows::Graphics::Effects::IGraphicsEffectSource>().detach();
        return S_OK;
    }
    IFACEMETHODIMP GetSourceCount(UINT* c) noexcept override { *c = (UINT)srcs.size(); return S_OK; }
};

struct GlassBrush : wux::Media::XamlCompositionBrushBaseT<GlassBrush> {
    wuc::Compositor comp; float blur; winrt::Windows::UI::Color tint;
    GlassBrush(wuc::Compositor c, float b, winrt::Windows::UI::Color t) : comp(std::move(c)), blur(b), tint(t) {}

    void OnConnected()
    {
        if (CompositionBrush()) return;
        try {
            auto blurFx = winrt::make<Effect>(CLSID_D2D1GaussianBlur, std::vector<wf::IInspectable>{
                PV::CreateSingle(blur), PV::CreateUInt32(0), PV::CreateUInt32(1) },
                std::vector<wge::IGraphicsEffectSource>{ wuc::CompositionEffectSourceParameter(L"Backdrop") });
            auto flood = winrt::make<Effect>(CLSID_D2D1Flood, std::vector<wf::IInspectable>{
                PV::CreateSingleArray({ tint.R / 255.f, tint.G / 255.f, tint.B / 255.f, tint.A / 255.f }) });
            auto fx = winrt::make<Effect>(CLSID_D2D1Composite, std::vector<wf::IInspectable>{ PV::CreateUInt32(0) },
                std::vector<wge::IGraphicsEffectSource>{ blurFx, flood });
            auto eb = comp.CreateEffectFactory(fx).CreateBrush();
            eb.SetSourceParameter(L"Backdrop", comp.CreateBackdropBrush());
            CompositionBrush(eb);
        }
        catch (winrt::hresult_error const& e) { Log(L"GlassBrush: " + Hex(e.code()) + L" " + std::wstring(e.message())); }
    }
    void OnDisconnected() { CompositionBrush(nullptr); }
};

enum class Kind { Glass, Clear, Round, Highlight, Dot, Light, Bright, Tile, Hide, Indicator, Clock };
struct Target {
    const wchar_t* type; const wchar_t* name;
    const wchar_t* ancestor; int depth;
    Kind kind; double radius;
    float blur;
    double padding = 0;
    double center = -1;
    bool clearInner = false;
};
static const Target g_targets[] = {
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundElement", L"Taskbar.TaskListButtonPanel", 1, Kind::Highlight, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundElement", L"Taskbar.TaskListButtonPanel", 2, Kind::Highlight, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundElement", L"IconPanel", 1, Kind::Highlight, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundElement", L"Taskbar.TaskListLabeledButtonPanel", 1, Kind::Highlight, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"MultiWindowElement", nullptr, 0, Kind::Hide, 0, 0 },
    { L"Windows.UI.Xaml.Shapes.Rectangle", L"RunningIndicator", L"IconPanel", 1, Kind::Indicator, 0, 0 },
    { L"Windows.UI.Xaml.Controls.TextBlock", L"TimeInnerTextBlock", L"DateTimeStackPanel", 1, Kind::Clock, 0, 0 },
    { L"Windows.UI.Xaml.Controls.TextBlock", L"DateInnerTextBlock", L"DateTimeStackPanel", 1, Kind::Clock, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundBorder", L"SystemTray.NotifyIconView", 2, Kind::Dot, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundBorder", L"SystemTray.ChevronIconView", 2, Kind::Dot, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundBorder", L"SystemTray.OmniButton", 2, Kind::Highlight, 0, 0 },
    { L"WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemList", nullptr, nullptr, 0, Kind::Glass, 40, 6 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundElement", L"ModalRootGrid", 1, Kind::Clear, 40, 0, 0, 24 },
    { L"Windows.UI.Xaml.Controls.Border", nullptr, L"WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemListViewItem", 2, Kind::Glass, 16, 18 },
    { L"Windows.UI.Xaml.Controls.Border", L"VirtualDesktopBarBackground", nullptr, 0, Kind::Glass, 40, 15 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundDimmingLayer", nullptr, 0, Kind::Glass, 0, 12 },
    { L"Windows.UI.Xaml.Controls.Border", L"ListBackground", nullptr, 0, Kind::Glass, 24, 15 },
    { L"Windows.UI.Xaml.Controls.Border", L"SnapBarBorder", nullptr, 0, Kind::Glass, 24, 5 },
    { L"Windows.UI.Xaml.Controls.Border", nullptr, L"Windows.UI.Xaml.Controls.MenuFlyoutPresenter", 1, Kind::Glass, 16, 5 },
    { L"Windows.UI.Xaml.Controls.ContentPresenter", L"LayoutRoot", L"Windows.UI.Xaml.Controls.ToolTip", 1, Kind::Glass, 14, 4 },
    { L"Windows.UI.Xaml.Controls.Border", L"HoverFlyoutBackground", nullptr, 0, Kind::Clear, 0, 0 },
    { L"Windows.UI.Xaml.Controls.ContentPresenter", L"HoverFlyoutContent", nullptr, 0, Kind::Clear, 0, 0, 6 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundBorder", L"Taskbar.TaskItemThumbnailView", 1, Kind::Round, 24, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"BackgroundBorder", L"Taskbar.TaskItemThumbnailView", 2, Kind::Round, 24, 0 },
    { L"Windows.UI.Xaml.Controls.Grid", nullptr, L"WindowsInternal.ComposableShell.Experiences.TextInput.Common.InputSwitcher", 3, Kind::Glass, 16, 5 },
    { L"Windows.UI.Xaml.Controls.Grid", nullptr, L"WindowsInternal.ComposableShell.Experiences.TextInput.Common.InputSwitcher", 4, Kind::Clear, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Border", L"OverflowFlyoutBackgroundBorder", nullptr, 0, Kind::Glass, 20, 5 },
    { L"Windows.UI.Xaml.Controls.Grid", L"ControlCenterRegion", nullptr, 0, Kind::Glass, 24, 5, 0, -1, true },
    { L"Windows.UI.Xaml.Controls.Grid", L"MediaTransportControlsRegion", nullptr, 0, Kind::Glass, 24, 5, 0, -1, true },
    { L"ControlCenter.MediaTransportControls", L"MediaTransportControls", nullptr, 0, Kind::Clear, 0, 0 },
    { L"Windows.UI.Xaml.Controls.Primitives.ToggleButton", nullptr, L"ControlCenterRegion", -24, Kind::Light, 0, 0 },
    { L"Windows.UI.Xaml.Controls.ContentPresenter", L"ContentPresenter", L"ControlCenter.PaginatedToggleButton", -3, Kind::Tile, 0, 0 },
    { L"Windows.UI.Xaml.Shapes.Rectangle", L"HorizontalTrackRect", nullptr, 0, Kind::Light, 0, 0 },
    { L"Windows.UI.Xaml.Shapes.Rectangle", L"HorizontalDecreaseRect", nullptr, 0, Kind::Bright, 0, 0 },
    { L"Windows.UI.Xaml.Shapes.Ellipse", L"SliderInnerThumb", nullptr, 0, Kind::Bright, 0, 0 },
};
constexpr double kFlyoutRadius = 24;
constexpr float kFlyoutBlur = 3.5f;

struct Surface : std::enable_shared_from_this<Surface> {
    winrt::weak_ref<wux::FrameworkElement> el;
    winrt::Windows::System::DispatcherQueue dq{ nullptr };
    int target;
    bool guarded = false;
    bool setting = false;
    winrt::Windows::Foundation::IUnknown oursBg{ nullptr }, oursBorder{ nullptr };
    ULONGLONG redoFrom = 0;
    int redos = 0;
    double baseFont = 0;
    std::vector<std::pair<wux::DependencyProperty, wf::IInspectable>> saved;
    winrt::Windows::System::DispatcherQueueTimer redoTimer{ nullptr };
    wux::FrameworkElement::Loaded_revoker loadedRev;
    wux::FrameworkElement::LayoutUpdated_revoker layoutRev, trayRev;
    wux::FrameworkElement::SizeChanged_revoker sizeRev;
    struct Refit { std::function<void()> run; ULONGLONG until = 0; };
    winrt::Windows::System::DispatcherQueueTimer refitTimer{ nullptr };
    std::shared_ptr<Refit> refit;
    ~Surface()
    {
        try { if (refitTimer) refitTimer.Stop(); } catch (...) {}
        try { if (redoTimer) redoTimer.Stop(); } catch (...) {}
    }
};
static std::vector<std::shared_ptr<Surface>> g_surfaces;

static wux::FrameworkElement Find(wux::DependencyObject const& root, std::wstring_view nameOrType)
{
    int n = VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < n; i++) {
        auto c = VisualTreeHelper::GetChild(root, i);
        if (auto fe = c.try_as<wux::FrameworkElement>(); fe && (fe.Name() == nameOrType || winrt::get_class_name(fe) == nameOrType))
            return fe;
        if (auto r = Find(c, nameOrType)) return r;
    }
    return nullptr;
}

static Rect Ensure(wux::Controls::Panel const& p, const wchar_t* name)
{
    for (auto ch : p.Children())
        if (auto fe = ch.try_as<wux::FrameworkElement>(); fe && fe.Name() == name) return fe.as<Rect>();
    Rect r;
    r.Name(name);
    r.IsHitTestVisible(false);
    p.Children().Append(r);
    return r;
}

static wux::Media::LinearGradientBrush Gradient(bool vertical, int rgb, std::initializer_list<std::pair<double, double>> stops)
{
    wux::Media::LinearGradientBrush g;
    g.StartPoint({ vertical ? 0.5f : 0.f, vertical ? 0.f : 0.5f });
    g.EndPoint({ vertical ? 0.5f : 1.f, vertical ? 1.f : 0.5f });
    for (auto [off, a] : stops) {
        wux::Media::GradientStop s;
        s.Offset(off);
        s.Color({ BYTE(std::clamp(a, 0.0, 1.0) * 255), BYTE(rgb >> 16), BYTE(rgb >> 8), BYTE(rgb) });
        g.GradientStops().Append(s);
    }
    return g;
}

static wux::Media::Brush MakeGlass(wux::UIElement const& host, float blur, winrt::Windows::UI::Color tint)
{
    if (blur < 0.05f) return wux::Media::SolidColorBrush(tint);
    return winrt::make<GlassBrush>(wux::Hosting::ElementCompositionPreview::GetElementVisual(host).Compositor(), blur, tint);
}

static winrt::Windows::UI::Color TintOf(Config const& c)
{
    auto t = c.tint;
    t.A = BYTE(std::clamp(c.tintOpacity, 0.0, 1.0) * 255);
    return t;
}

static wux::Media::Brush Rim(Config const& c)
{
    return Gradient(true, c.rimColor, { { 0, cfg::RimAlpha(cfg::Specular, c.specular) }, { 0.5, cfg::RimMid(c.specular, c.glow) },
                            { 1, cfg::RimAlpha(cfg::BorderGlow, c.glow) } });
}
static void Shadow(wux::Controls::Panel const& panel, Config const& c, double radius, bool on)
{
    constexpr int kRings = 6;
    for (int k = 1; k <= kRings; k++) {
        auto name = L"GlassShadow" + std::to_wstring(k);
        double a = on ? c.shadow * 0.5 * std::pow(1 - (k - 1.0) / kRings, 2) : 0;
        Rect r{ nullptr };
        for (auto ch : panel.Children())
            if (auto fe = ch.try_as<wux::FrameworkElement>(); fe && fe.Name() == name) r = fe.as<Rect>();
        if (!r && a <= 0) continue;
        if (!r) r = Ensure(panel, name.c_str());
        r.Visibility(a > 0 ? wux::Visibility::Visible : wux::Visibility::Collapsed);
        r.Stroke(wux::Media::SolidColorBrush({ BYTE(a * 255), 0, 0, 0 }));
        r.StrokeThickness(1);
        r.Margin({ -double(k), -double(k), -double(k), -double(k) });
        r.RadiusX(radius + k);
        r.RadiusY(radius + k);
    }
}

static void StyleGlass(wux::Controls::Panel const& panel, Rect const& fill, Config const& c, double radius, float blur, bool shadow = false)
{
    fill.Fill(MakeGlass(fill, blur, TintOf(c)));

    auto lens = Ensure(panel, L"GlassLens");
    double t = c.refraction * 10;
    lens.Fill(nullptr);
    lens.Visibility(t >= 0.5 ? wux::Visibility::Visible : wux::Visibility::Collapsed);
    lens.Stroke(MakeGlass(lens, blur * 3 + 12, { BYTE(c.refraction * 18), 255, 255, 255 }));
    lens.StrokeThickness(t);

    auto rim = Ensure(panel, L"GlassRim");
    rim.Stroke(Rim(c));
    rim.StrokeThickness(c.rimWidth);

    for (auto e : { fill, lens, rim }) { e.RadiusX(radius); e.RadiusY(radius); }
    Shadow(panel, c, radius, shadow);
}

static winrt::Windows::Foundation::IUnknown Identity(wux::Media::Brush const& b)
{
    return b ? b.as<winrt::Windows::Foundation::IUnknown>() : nullptr;
}

static std::pair<wux::Media::Brush, wux::Media::Brush> Brushes(wux::DependencyObject const& o)
{
    if (auto x = o.try_as<wux::Controls::Border>()) return { x.Background(), x.BorderBrush() };
    if (auto x = o.try_as<wux::Controls::Control>()) return { x.Background(), x.BorderBrush() };
    if (auto x = o.try_as<wux::Controls::Grid>()) return { x.Background(), x.BorderBrush() };
    if (auto x = o.try_as<wux::Controls::ContentPresenter>()) return { x.Background(), x.BorderBrush() };
    if (auto x = o.try_as<wux::Controls::Panel>()) return { x.Background(), nullptr };
    return { nullptr, nullptr };
}

static void GlassBox(wux::FrameworkElement const& e, Config const& c, double r, bool glass, float blur)
{
    wux::Media::Brush clear = wux::Media::SolidColorBrush(winrt::Windows::UI::Colors::Transparent());
    wux::Media::Brush bg = glass ? MakeGlass(e, blur, TintOf(c)) : clear;
    wux::Media::Brush border = glass && r > 0 ? Rim(c) : clear;
    wux::Thickness th = glass && r > 0 ? wux::Thickness{ c.rimWidth, c.rimWidth, c.rimWidth, c.rimWidth } : wux::Thickness{};
    wux::CornerRadius cr{ r, r, r, r };
    auto set = [&](auto const& x) {
        x.Background(bg);
        x.BorderBrush(border);
        x.BorderThickness(th);
        if (r > 0) x.CornerRadius(cr);
    };
    if (auto x = e.try_as<wux::Controls::Border>()) set(x);
    else if (auto x = e.try_as<wux::Controls::Control>()) set(x);
    else if (auto x = e.try_as<wux::Controls::Grid>()) set(x);
    else if (auto x = e.try_as<wux::Controls::ContentPresenter>()) set(x);
    else if (auto x = e.try_as<wux::Controls::Panel>()) x.Background(bg);
}

static void ClearInnerLayers(wux::DependencyObject const& o, double fullWidth, int depth)
{
    if (depth > 8 || fullWidth <= 0) return;
    for (int i = 0, n = VisualTreeHelper::GetChildrenCount(o); i < n; i++) {
        auto ch = VisualTreeHelper::GetChild(o, i);
        auto fe = ch.try_as<wux::FrameworkElement>();
        bool ours = false;
        if (fe) for (auto& t : g_targets) ours |= t.kind == Kind::Glass && t.name && fe.Name() == t.name;
        if (ours) continue;
        if (fe && fe.ActualWidth() >= fullWidth * 0.9) {
            auto clearIfTinted = [](auto const& x) {
                auto br = x.Background();
                auto s = br.try_as<wux::Media::SolidColorBrush>();
                if (s && s.Color().A > 0)
                    x.Background(wux::Media::SolidColorBrush(winrt::Windows::UI::Colors::Transparent()));
            };
            if (auto x = ch.try_as<wux::Controls::Border>()) clearIfTinted(x);
            else if (auto x = ch.try_as<wux::Controls::Panel>()) clearIfTinted(x);
            else if (auto x = ch.try_as<wux::Controls::Control>()) clearIfTinted(x);
            else if (auto x = ch.try_as<wux::Controls::ContentPresenter>()) clearIfTinted(x);
        }
        ClearInnerLayers(ch, fullWidth, depth + 1);
    }
}

struct Glide { double fromX, fromW, toX, toW; ULONGLONG t0; int delayMs, durMs; };
thread_local std::map<void*, Glide> t_glides;

static double GlideProgress(Glide const& g)
{
    double t = (double(GetTickCount64() - g.t0) - g.delayMs) / g.durMs;
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return 1 - std::pow(1 - t, 3);
}

static void Place(wux::FrameworkElement const& e, double x, double w, double mv, double anim)
{
    auto m = e.Margin();
    double oldW = e.Width(), oldX = m.Left;
    bool moved = !(std::abs(oldW - w) <= 0.5 && std::abs(oldX - x) <= 0.5);
    if (!moved && m.Top == mv && m.Bottom == mv) return;
    e.Width(w);
    e.Margin({ x, mv, 0, mv });
    if (!moved || std::isnan(oldW) || w <= 0 || anim < 1) return;

    try {
        std::erase_if(t_glides, [](auto const& kv) { return GlideProgress(kv.second) >= 1; });
        double fromX = oldX, fromW = oldW;
        if (auto it = t_glides.find(winrt::get_abi(e)); it != t_glides.end()) {
            double p = GlideProgress(it->second);
            fromX = it->second.fromX + (it->second.toX - it->second.fromX) * p;
            fromW = it->second.fromW + (it->second.toW - it->second.fromW) * p;
        }
        bool shrinking = w < fromW - 0.5;
        int delayMs = shrinking ? int(anim * 0.8) : 0, durMs = int(shrinking ? anim * 2 : anim);
        t_glides[winrt::get_abi(e)] = { fromX, fromW, x, w, GetTickCount64(), delayMs, durMs };

        auto v = wux::Hosting::ElementCompositionPreview::GetElementVisual(e);
        wux::Hosting::ElementCompositionPreview::SetIsTranslationEnabled(e, true);
        auto comp = v.Compositor();
        auto ease = comp.CreateCubicBezierEasingFunction({ 0.33f, 1.f }, { 0.68f, 1.f });
        auto glide = [&](const wchar_t* prop, float from, float to) {
            auto a = comp.CreateScalarKeyFrameAnimation();
            a.InsertKeyFrame(0.f, from);
            a.InsertKeyFrame(1.f, to, ease);
            a.Duration(std::chrono::milliseconds(durMs));
            a.DelayTime(std::chrono::milliseconds(delayMs));
            a.DelayBehavior(wuc::AnimationDelayBehavior::SetInitialValueBeforeDelay);
            v.StartAnimation(prop, a);
        };
        v.CenterPoint({ 0, 0, 0 });
        glide(L"Scale.X", float(fromW / w), 1.f);
        glide(L"Translation.X", float(fromX - x), 0.f);
    } catch (...) {}
}

static void Apply(Surface& b);

static std::atomic_bool g_reapply;
static void ReapplyAll()
{
    if (g_reapply.exchange(true)) return;
    std::lock_guard l(g_lock);
    for (auto& s : g_surfaces) s->dq.TryEnqueue([s] { Apply(*s); });
    g_reapply = false;
}

constexpr BYTE kOn = 0xCC;

static void HoldFill(Surface& b, wux::Shapes::Shape const& x, winrt::Windows::UI::Color want)
{
    x.Fill(wux::Media::SolidColorBrush(want));
    if (b.guarded) return;
    b.guarded = true;
    x.RegisterPropertyChangedCallback(wux::Shapes::Shape::FillProperty(), [want](wux::DependencyObject const& o, wux::DependencyProperty const&) {
        try {
            auto sh = o.try_as<wux::Shapes::Shape>();
            auto s = sh ? sh.Fill().try_as<wux::Media::SolidColorBrush>() : nullptr;
            if (s && s.Color() != want) s.Color(want);
        } catch (...) {}
    });
}

struct Tinted { winrt::weak_ref<wux::Media::SolidColorBrush> brush; winrt::Windows::UI::Color orig; bool hover; };
thread_local std::vector<Tinted> t_tinted;
thread_local bool t_tinting = false;

static Config Current()
{
    std::lock_guard l(g_lock);
    return g_cfg;
}

static winrt::Windows::UI::Color Rgb(int rgb, BYTE a) { return { a, BYTE(rgb >> 16), BYTE(rgb >> 8), BYTE(rgb) }; }

static winrt::Windows::UI::Color Want(Tinted const& t, Config const& c)
{
    auto o = t.orig;
    if (!t.hover) {
        if (!c.indicator) return { 0, o.R, o.G, o.B };
        return c.indColor == cfg::Stock ? o : Rgb(c.indColor, o.A);
    }
    BYTE a = BYTE(std::clamp(o.A * c.hover, 0.0, 255.0));
    return c.hoverColor == cfg::Stock ? winrt::Windows::UI::Color{ a, o.R, o.G, o.B } : Rgb(c.hoverColor, a);
}

static void Retint(Tinted& t, Config const& c)
{
    auto brush = t.brush.get();
    if (!brush) return;
    auto want = Want(t, c);
    if (brush.Color() == want) return;
    t_tinting = true;
    try { brush.Color(want); } catch (...) {}
    t_tinting = false;
}

static void RetintAll(Config const& c)
{
    for (auto& t : t_tinted) Retint(t, c);
}

static void Tint(wux::Media::Brush const& br, bool hover)
{
    auto s = br ? br.try_as<wux::Media::SolidColorBrush>() : nullptr;
    if (!s) return;
    auto c = Current();
    auto id = Identity(s);
    std::erase_if(t_tinted, [](Tinted const& t) { return !t.brush.get(); });
    for (auto& t : t_tinted)
        if (Identity(t.brush.get()) == id) { Retint(t, c); return; }
    t_tinted.push_back({ winrt::make_weak(s), s.Color(), hover });
    s.RegisterPropertyChangedCallback(wux::Media::SolidColorBrush::ColorProperty(), [](wux::DependencyObject const& o, wux::DependencyProperty const&) {
        if (t_tinting) return;
        auto id = o.as<winrt::Windows::Foundation::IUnknown>();
        for (auto& t : t_tinted)
            if (auto b = t.brush.get(); b && Identity(b) == id) { t.orig = b.Color(); Retint(t, Current()); return; }
    });
    Retint(t_tinted.back(), c);
}

static void TintHover(Surface& b, wux::FrameworkElement const& bg, Config const& c)
{
    auto x = bg.try_as<wux::Controls::Border>();
    if (!x) return;
    RetintAll(c);
    Tint(x.Background(), true);
    if (b.guarded) return;
    b.guarded = true;
    x.RegisterPropertyChangedCallback(wux::Controls::Border::BackgroundProperty(), [](wux::DependencyObject const& o, wux::DependencyProperty const&) {
        try { Tint(o.as<wux::Controls::Border>().Background(), true); } catch (...) {}
    });
}

static void TintIndicator(Surface& b, wux::FrameworkElement const& bg, Config const& c)
{
    auto x = bg.try_as<wux::Shapes::Shape>();
    if (!x) return;
    RetintAll(c);
    Tint(x.Fill(), false);
    if (b.guarded) return;
    b.guarded = true;
    x.RegisterPropertyChangedCallback(wux::Shapes::Shape::FillProperty(), [](wux::DependencyObject const& o, wux::DependencyProperty const&) {
        try { Tint(o.as<wux::Shapes::Shape>().Fill(), false); } catch (...) {}
    });
}

static winrt::Windows::Foundation::IUnknown Id(wf::IInspectable const& x)
{
    return x ? x.as<winrt::Windows::Foundation::IUnknown>() : nullptr;
}

static void Keep(Surface& b, wux::DependencyObject const& o, wux::DependencyProperty const& p)
{
    for (auto& s : b.saved)
        if (Id(s.first) == Id(p)) return;
    b.saved.emplace_back(p, o.ReadLocalValue(p));
}

static void Restore(Surface& b, wux::DependencyObject const& o, wux::DependencyProperty const& p)
{
    for (auto& s : b.saved) {
        if (Id(s.first) != Id(p)) continue;
        if (Id(s.second) == Id(wux::DependencyProperty::UnsetValue())) o.ClearValue(p);
        else try { o.SetValue(p, s.second); } catch (...) { o.ClearValue(p); }
    }
}

static void Clock(Surface& b, wux::Controls::TextBlock const& x, Config const& c)
{
    using P = wux::Controls::TextBlock;
    if (!b.baseFont) b.baseFont = x.FontSize();
    if (std::abs(c.clockSize - 1) < 1e-3) Restore(b, x, P::FontSizeProperty());
    else { Keep(b, x, P::FontSizeProperty()); x.FontSize(b.baseFont * c.clockSize); }
    if (!c.clockBold) Restore(b, x, P::FontWeightProperty());
    else { Keep(b, x, P::FontWeightProperty()); x.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold()); }
    if (c.clockColor == cfg::Stock) Restore(b, x, P::ForegroundProperty());
    else { Keep(b, x, P::ForegroundProperty()); x.Foreground(wux::Media::SolidColorBrush(Rgb(c.clockColor, 255))); }
}

thread_local std::vector<winrt::weak_ref<wux::UIElement>> t_nudged;
thread_local ULONGLONG t_nudgeUntil = 0;
thread_local bool t_flip = false;
thread_local wux::Media::CompositionTarget::Rendering_revoker t_frames;

static void Nudge()
{
    auto dq = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
    std::vector<std::shared_ptr<Surface>> mine;
    {
        std::lock_guard l(g_lock);
        for (auto& s : g_surfaces)
            if (s->dq == dq && (s->target < 0 || g_targets[s->target].kind == Kind::Glass)) mine.push_back(s);
    }
    std::vector<winrt::weak_ref<wux::UIElement>> els;
    for (auto& s : mine) try {
        auto fe = s->el.get();
        if (!fe || fe.Opacity() < 0.99) continue;
        wux::DependencyObject e = fe;
        while (s->target < 0 && e && winrt::get_class_name(e) != L"Taskbar.TaskbarFrame") e = VisualTreeHelper::GetParent(e);
        auto root = s->target < 0 && e ? VisualTreeHelper::GetParent(e).try_as<wux::UIElement>() : fe;
        if (root) els.push_back(winrt::make_weak(root));
        if (auto pill = s->target < 0 && root ? Find(root, L"GlassTrayPill") : nullptr) {
            els.push_back(winrt::make_weak(pill.as<wux::UIElement>()));
            for (auto ch : pill.as<wux::Controls::Panel>().Children()) els.push_back(winrt::make_weak(ch));
        }
    } catch (...) {}
    if (els.empty()) return;
    t_nudged = std::move(els);
    t_nudgeUntil = GetTickCount64() + 700;
    if (t_frames) return;
    t_frames = wux::Media::CompositionTarget::Rendering(winrt::auto_revoke, [](auto&&, auto&&) {
        bool done = GetTickCount64() > t_nudgeUntil;
        t_flip = !t_flip;
        for (auto& w : t_nudged) try {
            if (auto e = w.get()) e.Opacity(done ? 1 : t_flip ? 0.999 : 0.998);
        } catch (...) {}
        if (done) t_frames = {};
    });
}

static bool OverVisibleBar(RECT const* wr)
{
    for (auto cls : { L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd" })
        for (HWND t = FindWindowExW(nullptr, nullptr, cls, nullptr); t; t = FindWindowExW(nullptr, t, cls, nullptr)) {
            RECT tr, vis, hit;
            MONITORINFO mi{ sizeof(mi) };
            if (!GetWindowRect(t, &tr) || !GetMonitorInfoW(MonitorFromWindow(t, MONITOR_DEFAULTTONEAREST), &mi)) continue;
            if (IntersectRect(&vis, &tr, &mi.rcMonitor) && vis.bottom - vis.top > 8 && (!wr || IntersectRect(&hit, &vis, wr))) return true;
        }
    return false;
}

static void CALLBACK OnWinEvent(HWINEVENTHOOK, DWORD ev, HWND w, LONG obj, LONG, DWORD, DWORD)
{
    if (obj != OBJID_WINDOW || !w || GetAncestor(w, GA_ROOT) != w) return;
    RECT wr;
    if (ev == EVENT_OBJECT_HIDE && !GetWindowRect(w, &wr)) return;
    if (OverVisibleBar(ev == EVENT_OBJECT_HIDE ? &wr : nullptr)) Nudge();
}

struct WinHooks {
    HWINEVENTHOOK minimize = nullptr, hide = nullptr;
    ~WinHooks() { if (minimize) UnhookWinEvent(minimize); if (hide) UnhookWinEvent(hide); }
};
thread_local WinHooks t_hooks;

static void WatchWindows()
{
    if (t_hooks.minimize) return;
    t_hooks.minimize = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr, OnWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    t_hooks.hide = SetWinEventHook(EVENT_OBJECT_HIDE, EVENT_OBJECT_HIDE, nullptr, OnWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
}

static void Apply(Surface& b) try
{
    auto bg = b.el.get();
    if (!bg) return;
    WatchWindows();
    Config c;
    { std::lock_guard l(g_lock); c = g_cfg; }

    if (b.target >= 0) {
        auto& t = g_targets[b.target];
        double tr = t.radius * c.radius / 20;
        wux::CornerRadius cr{ tr, tr, tr, tr };
        wux::Thickness pad{ t.padding, t.padding, t.padding, t.padding };
        c.tintOpacity = c.panelTint;
        if (t.kind == Kind::Hide) {
            bg.Opacity(0);
        } else if (t.kind == Kind::Dot) {
            double d = std::clamp(48 - 2 * (c.marginV + 2), 16.0, 30.0);
            double dr = std::min(d / 2, c.hlRadius);
            if (auto x = bg.try_as<wux::Controls::Border>()) x.CornerRadius({ dr, dr, dr, dr });
            bg.Width(d);
            bg.Height(d);
            bg.Margin({ 0, 0, 0, 0 });
            bg.HorizontalAlignment(wux::HorizontalAlignment::Center);
            bg.VerticalAlignment(wux::VerticalAlignment::Center);
            TintHover(b, bg, c);
        } else if (t.kind == Kind::Highlight) {
            double r = std::min(c.hlRadius, std::max(4.0, c.radius - 2)), v = c.marginV + 2;
            if (auto x = bg.try_as<wux::Controls::Border>()) x.CornerRadius({ r, r, r, r });
            bg.Margin({ 2, v, 2, v });
            bg.VerticalAlignment(wux::VerticalAlignment::Stretch);
            TintHover(b, bg, c);
        } else if (t.kind == Kind::Indicator) {
            TintIndicator(b, bg, c);
        } else if (t.kind == Kind::Clock) {
            if (auto x = bg.try_as<wux::Controls::TextBlock>()) Clock(b, x, c);
        } else if (t.kind == Kind::Bright) {
            auto outer = VisualTreeHelper::GetParent(bg).try_as<wux::Controls::Border>();
            if (bg.Name() == L"SliderInnerThumb" && outer) {
                bg.Opacity(0);
                outer.Width(NAN);
                outer.Height(NAN);
                outer.Margin({ 0, 0, 0, 0 });
                outer.BorderThickness({ 0, 0, 0, 0 });
                outer.CornerRadius({ 10, 10, 10, 10 });
                outer.Background(wux::Media::SolidColorBrush(winrt::Windows::UI::Colors::White()));
            } else if (auto x = bg.try_as<wux::Shapes::Shape>()) HoldFill(b, x, { kOn, 255, 255, 255 });
        } else if (t.kind == Kind::Tile) {
            auto cp = bg.try_as<wux::Controls::ContentPresenter>();
            if (cp && bg.ActualTheme() == wux::ElementTheme::Dark) {
                cp.Background(wux::Media::SolidColorBrush({ 0x20, 255, 255, 255 }));
                auto whiten = [](wux::DependencyObject const& o, wux::DependencyProperty const&) {
                    auto x = o.try_as<wux::Controls::ContentPresenter>();
                    auto s = x ? x.Background().try_as<wux::Media::SolidColorBrush>() : nullptr;
                    if (!s) return;
                    auto c = s.Color();
                    if (c.A >= 0x80 && !(c.R == 255 && c.G == 255 && c.B == 255)) s.Color({ BYTE(c.A * kOn / 255), 255, 255, 255 });
                };
                if (!b.guarded) {
                    b.guarded = true;
                    cp.RegisterPropertyChangedCallback(wux::Controls::ContentPresenter::BackgroundProperty(), whiten);
                }
                whiten(cp, nullptr);
            }
        } else if (t.kind == Kind::Light) {
            auto white = [](BYTE a) { return wux::Media::SolidColorBrush({ a, 255, 255, 255 }); };
            if (auto x = bg.try_as<Rect>()) HoldFill(b, x, { 0x48, 255, 255, 255 });
            else if (auto x = bg.try_as<wux::Controls::Control>()) { x.Background(white(0x20)); x.BorderBrush(white(0x30)); }
        } else if (t.kind == Kind::Round) {
            if (auto x = bg.try_as<wux::Controls::Border>()) x.CornerRadius(cr);
            else if (auto x = bg.try_as<wux::Controls::Control>()) x.CornerRadius(cr);
        } else {
            b.setting = true;
            GlassBox(bg, c, tr, t.kind == Kind::Glass, t.blur * c.blur / 3);
            b.setting = false;
            {
                auto [back, border] = Brushes(bg);
                b.oursBg = Identity(back);
                b.oursBorder = Identity(border);
            }
            if (t.kind == Kind::Glass && !b.guarded) {
                b.guarded = true;
                b.redoTimer = b.dq.CreateTimer();
                b.redoTimer.IsRepeating(false);
                b.redoTimer.Interval(std::chrono::milliseconds(300));
                b.redoTimer.Tick([wp = b.weak_from_this()](auto&&, auto&&) {
                    auto p = wp.lock();
                    if (!p) return;
                    auto e = p->el.get();
                    if (!e) return;
                    auto [back, border] = Brushes(e);
                    if (Identity(back) == p->oursBg && Identity(border) == p->oursBorder) return;
                    ULONGLONG now = GetTickCount64();
                    if (now - p->redoFrom > 10000) { p->redoFrom = now; p->redos = 0; }
                    Log(L"panel look replaced by the shell: " + std::wstring(winrt::get_class_name(e)) + L" #" + std::wstring(e.Name()) + L" target " + std::to_wstring(p->target) + L", re-style " + std::to_wstring(p->redos + 1));
                    if (p->redos++ < 5) ReapplyAll();
                });
                auto redo = [wp = b.weak_from_this()](wux::DependencyObject const&, wux::DependencyProperty const&) {
                    auto p = wp.lock();
                    if (!p || p->setting) return;
                    try { p->redoTimer.Start(); } catch (...) {}
                };
                auto watch = [&](auto const& x, wux::DependencyProperty const& back, wux::DependencyProperty const& border) {
                    x.RegisterPropertyChangedCallback(back, redo);
                    x.RegisterPropertyChangedCallback(border, redo);
                };
                if (auto x = bg.try_as<wux::Controls::Border>()) watch(x, wux::Controls::Border::BackgroundProperty(), wux::Controls::Border::BorderBrushProperty());
                else if (auto x = bg.try_as<wux::Controls::Control>()) watch(x, wux::Controls::Control::BackgroundProperty(), wux::Controls::Control::BorderBrushProperty());
                else if (auto x = bg.try_as<wux::Controls::Grid>()) watch(x, wux::Controls::Panel::BackgroundProperty(), wux::Controls::Grid::BorderBrushProperty());
                else if (auto x = bg.try_as<wux::Controls::ContentPresenter>()) watch(x, wux::Controls::ContentPresenter::BackgroundProperty(), wux::Controls::ContentPresenter::BorderBrushProperty());
                else if (auto x = bg.try_as<wux::Controls::Panel>()) x.RegisterPropertyChangedCallback(wux::Controls::Panel::BackgroundProperty(), redo);
            }
            if (auto lv = bg.try_as<wux::Controls::ListViewBase>()) {
                wux::Controls::ScrollViewer::SetVerticalScrollBarVisibility(lv, wux::Controls::ScrollBarVisibility::Hidden);
                wux::Controls::ScrollViewer::SetHorizontalScrollBarVisibility(lv, wux::Controls::ScrollBarVisibility::Hidden);
                if (auto sv = Find(lv, L"Windows.UI.Xaml.Controls.ScrollViewer").try_as<wux::Controls::ScrollViewer>()) {
                    sv.VerticalScrollBarVisibility(wux::Controls::ScrollBarVisibility::Hidden);
                    sv.HorizontalScrollBarVisibility(wux::Controls::ScrollBarVisibility::Hidden);
                }
            }
            if (t.kind == Kind::Clear && !b.guarded) {
                b.guarded = true;
                auto reclear = [](wux::DependencyObject const& o, wux::DependencyProperty const&) {
                    auto isClear = [](wux::Media::Brush const& br) {
                        auto s = br.try_as<wux::Media::SolidColorBrush>();
                        return s && s.Color().A == 0;
                    };
                    auto clear = wux::Media::SolidColorBrush(winrt::Windows::UI::Colors::Transparent());
                    if (auto x = o.try_as<wux::Controls::Border>(); x && !isClear(x.Background())) x.Background(clear);
                    else if (auto x = o.try_as<wux::Controls::Control>(); x && !isClear(x.Background())) x.Background(clear);
                };
                if (auto x = bg.try_as<wux::Controls::Border>()) x.RegisterPropertyChangedCallback(wux::Controls::Border::BackgroundProperty(), reclear);
                else if (auto x = bg.try_as<wux::Controls::Control>()) x.RegisterPropertyChangedCallback(wux::Controls::Control::BackgroundProperty(), reclear);
            }
        }
        if (t.clearInner) {
            ClearInnerLayers(bg, bg.ActualWidth(), 0);
            if (!b.sizeRev)
                b.sizeRev = bg.SizeChanged(winrt::auto_revoke, [](auto const& s, wux::SizeChangedEventArgs const& e) {
                    try { ClearInnerLayers(s.as<wux::DependencyObject>(), e.NewSize().Width, 0); } catch (...) {}
                });
        }
        if (t.padding > 0) {
            if (auto x = bg.try_as<wux::Controls::ContentPresenter>()) x.Padding(pad);
            else if (auto x = bg.try_as<wux::Controls::Border>()) x.Padding(pad);
            else if (auto x = bg.try_as<wux::Controls::Control>()) x.Padding(pad);
        }
        if (t.center >= 0) {
            bg.HorizontalAlignment(wux::HorizontalAlignment::Center);
            bg.Margin({ t.center, bg.Margin().Top, t.center, bg.Margin().Bottom });
        }
        return;
    }

    auto fill = Find(bg, L"BackgroundFill").try_as<Rect>();
    auto panel = fill ? VisualTreeHelper::GetParent(fill).try_as<wux::Controls::Panel>() : nullptr;
    if (!panel) {
        Log(fill ? L"Apply: BackgroundFill parent is not a Panel" : L"Apply: BackgroundFill not found");
        return;
    }

    if (auto stock = Find(bg, L"BackgroundStroke")) stock.Visibility(wux::Visibility::Collapsed);

    auto parent = VisualTreeHelper::GetParent(bg).try_as<wux::UIElement>();
    auto rep = parent ? Find(parent, L"TaskbarFrameRepeater") : nullptr;
    if (!rep) {
        c.tintOpacity = c.panelTint;
        StyleGlass(panel, fill, c, kFlyoutRadius * c.radius / 20, kFlyoutBlur * c.blur / 3);
        return;
    }

    if (!b.refit) b.refit = std::make_shared<Surface::Refit>();
    if (!b.refitTimer) {
        b.refitTimer = b.dq.CreateTimer();
        b.refitTimer.Interval(std::chrono::milliseconds(120));
        b.refitTimer.Tick([r = b.refit](auto const& t, auto&&) {
            try { if (r->run) r->run(); } catch (...) {}
            if (GetTickCount64() > r->until) t.Stop();
        });
    }
    auto kick = [r = b.refit, t = b.refitTimer] {
        r->until = GetTickCount64() + 1500;
        if (!t.IsRunning()) t.Start();
    };
    std::vector<std::function<void()>> fits;
    b.layoutRev = {};
    bool dynamic = c.dynamicWidth && !c.fullWidth;
    double width = c.fullWidth ? 0 : c.width;
    if (dynamic) {
        bg.HorizontalAlignment(wux::HorizontalAlignment::Left);
        auto fit = [wbg = winrt::make_weak(bg), wrep = winrt::make_weak(rep), wpar = winrt::make_weak(parent), pad = c.padding, mv = c.marginV, an = c.anim] {
            auto e = wbg.get(); auto r = wrep.get(); auto p = wpar.get();
            if (!e || !r || !p) return;
            double l = INFINITY, rt = -INFINITY;
            for (int i = 0, n = VisualTreeHelper::GetChildrenCount(r); i < n; i++) {
                auto ch = VisualTreeHelper::GetChild(r, i).try_as<wux::FrameworkElement>();
                if (!ch || ch.Visibility() != wux::Visibility::Visible || ch.ActualWidth() <= 0) continue;
                auto pt = ch.TransformToVisual(p).TransformPoint({ 0, 0 });
                if (pt.X < -1000) continue;
                l = std::min(l, (double)pt.X);
                rt = std::max(rt, pt.X + ch.ActualWidth());
            }
            if (l < rt) Place(e, std::max(0.0, l - pad), rt - l + 2 * pad, mv, an);
        };
        fit();
        fits.push_back(fit);
        b.layoutRev = rep.LayoutUpdated(winrt::auto_revoke, [fit, kick](auto&&, auto&&) { fit(); kick(); });
    } else {
        bg.HorizontalAlignment(width > 0 ? wux::HorizontalAlignment::Center : wux::HorizontalAlignment::Stretch);
        bg.Margin({ 0, c.marginV, 0, c.marginV });
        bg.Width(width > 0 ? width : NAN);
    }

    StyleGlass(panel, fill, c, c.radius, c.blur, true);

    wux::DependencyObject frame = bg;
    while (frame && winrt::get_class_name(frame) != L"Taskbar.TaskbarFrame") frame = VisualTreeHelper::GetParent(frame);
    auto root = frame ? VisualTreeHelper::GetParent(frame).try_as<wux::Controls::Grid>() : nullptr;
    if (root) {
        wux::Media::TranslateTransform shift;
        shift.Y(-std::clamp(c.offsetY, -c.marginV, c.marginV));
        root.RenderTransform(shift);
    }
    auto tray = root ? Find(root, L"SystemTrayFrameGrid") : nullptr;
    b.trayRev = {};
    if (tray) {
        wux::Controls::Grid pill{ nullptr };
        for (auto ch : root.Children())
            if (auto g = ch.try_as<wux::Controls::Grid>(); g && g.Name() == L"GlassTrayPill") pill = g;
        if (!pill) {
            pill = wux::Controls::Grid();
            pill.Name(L"GlassTrayPill");
            pill.IsHitTestVisible(false);
            pill.HorizontalAlignment(wux::HorizontalAlignment::Left);
            wux::Controls::Grid::SetColumnSpan(pill, 64);
            wux::Controls::Grid::SetRowSpan(pill, 64);
            root.Children().InsertAt(0, pill);
        }
        pill.Visibility(dynamic || width > 0 ? wux::Visibility::Visible : wux::Visibility::Collapsed);
        StyleGlass(pill, Ensure(pill, L"GlassFill"), c, c.radius, c.blur, true);
        auto fitTray = [wp = winrt::make_weak(pill), wt = winrt::make_weak(tray), wr = winrt::make_weak(root), pad = c.padding, mv = c.marginV, an = c.anim] {
            auto p = wp.get(); auto t = wt.get(); auto r = wr.get();
            if (!p || !t || !r || t.ActualWidth() <= 0) return;
            double x = std::max(0.0, t.TransformToVisual(r).TransformPoint({ 0, 0 }).X - pad);
            Place(p, x, std::min(r.ActualWidth() - x, t.ActualWidth() + 2 * pad), mv, an);
        };
        fitTray();
        fits.push_back(fitTray);
        b.trayRev = tray.LayoutUpdated(winrt::auto_revoke, [fitTray, kick](auto&&, auto&&) { fitTray(); kick(); });
    }
    b.refit->run = [fits] { for (auto& f : fits) f(); };
    kick();

    Log(std::wstring(L"Apply: ok, tray=") + (tray ? L"found" : L"NOT found") +
        L", bg width=" + std::to_wstring(bg.Width()) + L", fill " + std::to_wstring(fill.ActualWidth()) + L"x" + std::to_wstring(fill.ActualHeight()));
}
catch (winrt::hresult_error const& e) { Log(L"Apply: exception " + Hex(e.code()) + L" " + std::wstring(e.message())); }
catch (...) { Log(L"Apply: unknown exception"); }

static DWORD WINAPI WatchConfig(void*)
{
    auto dir = g_ini.substr(0, g_ini.rfind(L'\\'));
    auto stamp = [] {
        WIN32_FILE_ATTRIBUTE_DATA a{};
        GetFileAttributesExW(g_ini.c_str(), GetFileExInfoStandard, &a);
        return ULARGE_INTEGER{ a.ftLastWriteTime.dwLowDateTime, a.ftLastWriteTime.dwHighDateTime }.QuadPart;
    };
    auto last = stamp();
    HANDLE h = FindFirstChangeNotificationW(dir.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME);
    while (h != INVALID_HANDLE_VALUE && WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0) {
        Sleep(200);
        FindNextChangeNotification(h);
        if (auto now = stamp(); now == last) continue;
        else last = now;
        Log(L"config reloaded");
        { std::lock_guard l(g_lock); g_cfg = LoadConfig(); }
        ReapplyAll();
    }
    if (h != INVALID_HANDLE_VALUE) FindCloseChangeNotification(h);
    return 0;
}

struct __declspec(uuid("A6F1C2D4-3B5E-4F7A-9C8D-1E2F3A4B5C6D")) Tap
    : winrt::implements<Tap, IObjectWithSite, IVisualTreeServiceCallback2> {
    winrt::com_ptr<IXamlDiagnostics> diag;

    IFACEMETHODIMP SetSite(IUnknown* site) noexcept override
    {
        diag = nullptr;
        if (!site) return S_OK;
        HRESULT hr = site->QueryInterface(diag.put());
        Log(L"SetSite: IXamlDiagnostics " + Hex(hr));
        if (FAILED(hr)) return hr;

        static std::once_flag once;
        std::call_once(once, [] {
            g_cfg = LoadConfig();
            CloseHandle(CreateThread(nullptr, 0, WatchConfig, nullptr, 0, nullptr));
            try {
                static winrt::Windows::UI::ViewManagement::UISettings ui;
                auto again = [](const wchar_t* why) {
                    Log(std::wstring(L"shell change: ") + why);
                    static std::atomic_int gen;
                    int mine = ++gen;
                    std::thread([mine] {
                        for (int ms : { 300, 1200, 3000, 6000 }) { Sleep(ms); if (gen != mine) return; ReapplyAll(); }
                    }).detach();
                };
                ui.ColorValuesChanged([again](auto&&, auto&&) { again(L"colours"); });
                ui.AdvancedEffectsEnabledChanged([again](auto&&, auto&&) { again(L"transparency"); });
                winrt::Windows::System::Power::PowerManager::EnergySaverStatusChanged([again](auto&&, auto&&) { again(L"energy saver"); });
            } catch (...) { Log(L"shell change events: not available"); }
        });
        std::thread([self = get_strong()] {
            auto vts = self->diag.try_as<IVisualTreeService3>();
            Log(L"AdviseVisualTreeChange: " + (vts ? Hex(vts->AdviseVisualTreeChange(self.get())) : L"no IVisualTreeService3"));
        }).detach();
        return S_OK;
    }
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv) noexcept override
    {
        return diag ? diag->QueryInterface(riid, ppv) : E_FAIL;
    }

    IFACEMETHODIMP OnVisualTreeChange(ParentChildRelation, VisualElement el, VisualMutationType m) noexcept override
    try {
        if (m != Add || !el.Type) return S_OK;

        wux::FrameworkElement fe{ nullptr };
        auto object = [&] {
            ::IInspectable* raw = nullptr;
            if (!fe && SUCCEEDED(diag->GetIInspectableFromHandle(el.Handle, &raw)))
                fe = wf::IInspectable(raw, winrt::take_ownership_from_abi).try_as<wux::FrameworkElement>();
            return fe;
        };
        auto ancestorOk = [&](Target const& t) {
            if (!t.ancestor) return true;
            auto is = [&](wux::DependencyObject const& p) {
                auto a = p ? p.try_as<wux::FrameworkElement>() : nullptr;
                return a && (a.Name() == t.ancestor || winrt::get_class_name(a) == t.ancestor);
            };
            wux::DependencyObject p = fe;
            if (t.depth < 0) {
                for (int i = 0; i < -t.depth && p; i++) if (is(p = VisualTreeHelper::GetParent(p))) return true;
                return false;
            }
            for (int i = 0; i < t.depth && p; i++) p = VisualTreeHelper::GetParent(p);
            return is(p);
        };
        int target = wcscmp(el.Type, L"Taskbar.TaskbarBackground") ? -2 : -1;
        for (int i = 0; target == -2 && i < ARRAYSIZE(g_targets); i++) {
            auto& t = g_targets[i];
            if (!wcscmp(el.Type, t.type) && (!t.name || (el.Name && !wcscmp(el.Name, t.name))) && object() && ancestorOk(t))
                target = i;
        }
        if (target == -2 || !object()) return S_OK;

        std::shared_ptr<Surface> s;
        {
            std::lock_guard l(g_lock);
            std::erase_if(g_surfaces, [](auto const& x) { return !x->el.get(); });
            for (auto& old : g_surfaces)
                if (old->el.get() == fe) return S_OK;
            s = g_surfaces.emplace_back(std::make_shared<Surface>());
            s->el = winrt::make_weak(fe);
            s->dq = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
            s->target = target;
        }
        s->loadedRev = fe.Loaded(winrt::auto_revoke, [w = std::weak_ptr(s)](auto&&, auto&&) { if (auto p = w.lock()) Apply(*p); });
        s->dq.TryEnqueue([s] { Apply(*s); });
        return S_OK;
    }
    catch (...) { Log(L"OnVisualTreeChange: exception"); return S_OK; }

    IFACEMETHODIMP OnElementStateChanged(InstanceHandle, VisualElementState, LPCWSTR) noexcept override { return S_OK; }
};

struct Factory : winrt::implements<Factory, IClassFactory> {
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) noexcept override
    {
        if (outer) return CLASS_E_NOAGGREGATION;
        try { return winrt::make_self<Tap>()->QueryInterface(riid, ppv); }
        catch (...) { return winrt::to_hresult(); }
    }
    IFACEMETHODIMP LockServer(BOOL) noexcept override { return S_OK; }
};

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv)
{
    if (clsid != __uuidof(Tap)) return CLASS_E_CLASSNOTAVAILABLE;
    try { return winrt::make_self<Factory>()->QueryInterface(riid, ppv); }
    catch (...) { return winrt::to_hresult(); }
}

STDAPI DllCanUnloadNow() { return S_FALSE; }

static DWORD WINAPI InitTap(void* selfRef)
{
    using InitFn = HRESULT(WINAPI*)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, CLSID, LPCWSTR);
    HMODULE xaml = LoadLibraryExW(L"Windows.UI.Xaml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto init = xaml ? (InitFn)GetProcAddress(xaml, "InitializeXamlDiagnosticsEx") : nullptr;
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(g_mod, path, MAX_PATH);

    HRESULT hr = E_FAIL;
    for (int attempt = 0; init && attempt < 60 && FAILED(hr); attempt++) {
        if (attempt) Sleep(1000);
        for (int i = 1; i <= 10 && FAILED(hr); i++) {
            auto ep = L"VisualDiagConnection" + std::to_wstring(i);
            hr = init(ep.c_str(), GetCurrentProcessId(), nullptr, path, __uuidof(Tap), nullptr);
            if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) break;
        }
        Log(L"InitializeXamlDiagnosticsEx attempt " + std::to_wstring(attempt + 1) + L" = " + Hex(hr));
    }
    if (!init) Log(L"InitializeXamlDiagnosticsEx not found in Windows.UI.Xaml.dll");
    auto ready = app::ReadyPrefix + std::to_wstring(GetCurrentProcessId());
    if (SUCCEEDED(hr))
        if (HANDLE e = OpenEventW(EVENT_MODIFY_STATE, FALSE, ready.c_str())) { SetEvent(e); CloseHandle(e); }
    FreeLibraryAndExitThread((HMODULE)selfRef, 0);
}

extern "C" LRESULT CALLBACK GetMsgProc(int code, WPARAM w, LPARAM l)
{
    static std::atomic_bool started;
    if (code >= 0 && !started.exchange(true)) {
        Log(L"hook fired in pid " + std::to_wstring(GetCurrentProcessId()));
        HMODULE self;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&GetMsgProc, &self);
        if (HANDLE t = CreateThread(nullptr, 0, InitTap, self, 0, nullptr)) CloseHandle(t);
    }
    return CallNextHookEx(nullptr, code, w, l);
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, void*)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_mod = mod;
        DisableThreadLibraryCalls(mod);
        wchar_t p[MAX_PATH];
        GetModuleFileNameW(mod, p, MAX_PATH);
        g_ini = p;
        g_ini = g_ini.substr(0, g_ini.rfind(L'\\') + 1) + L"config.ini";
        auto log = g_ini.substr(0, g_ini.rfind(L'\\') + 1) + L"OpalTap.log";
        WIN32_FILE_ATTRIBUTE_DATA a{};
        if (GetFileAttributesExW(log.c_str(), GetFileExInfoStandard, &a) && (a.nFileSizeHigh || a.nFileSizeLow > (1u << 20)))
            DeleteFileW(log.c_str());
    }
    return TRUE;
}
